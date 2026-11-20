/*---------------------------------------------------------*\
| QMKKeychronController.cpp                                 |
|                                                           |
|   Driver for Keychron QMK-based keyboards                 |
|                                                           |
|   Amadej Kastelic                             21 Jun 2026 |
|   Adam Honse <calcprogrammer1@gmail.com>      22 Jun 2026 |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <chrono>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "hsv.h"
#include "QMKKeychronController.h"
#include "QMKKeychronController_Devices.h"
#include "QMKViaCommands.h"
#include "StringUtils.h"
#include "LogManager.h"
#include "QMKKeycodes.h"

#define KEYCHRON_PROBE_PREFIX                       "[QMK Keychron probe]"
#define KEYCHRON_PROBE_FRAMES                       3
#define KEYCHRON_PROBE_RECHECK_MS                   3000
#define KEYCHRON_PROBE_MAX_FAILURE_LOGS             40
#define KEYCHRON_RESEND_DELAY_MS                    5000

using namespace std::chrono_literals;

static long long KeychronNowMs()
{
    return(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}


QMKKeychronController::QMKKeychronController(hid_device* dev_handle, const char *path, unsigned short dev_pid)
{
    /*-----------------------------------------------------*\
    | Initialize controller fields                          |
    \*-----------------------------------------------------*/
    dev                         = dev_handle;
    location                    = path;
    kc_dongle_firmware_version  = "";
    kc_firmware_version         = "";
    kc_protocol_version         = 0;
    kc_rgb_protocol_version     = 0;
    number_leds                 = 0;
    supported_features          = 0;
    via_protocol_version        = 0;
    wireless_device_pid         = 0;
    wireless_device_vid         = 0;
    direct_mode                 = false;
    last_frame_brightness       = -1;
    last_warning_ms             = 0;
    per_key_effect              = 0;
    suppressed_warnings         = 0;
    via_reply_timeout           = KEYCHRON_QHE_HID_READ_TIMEOUT;
    saved_mode                  = 0;
    saved_speed                 = 0;
    saved_hue                   = 0;
    saved_sat                   = 0;
    saved_val                   = 0;
    resend_armed                = false;
    resend_done                 = false;
    resend_active               = false;
    resend_due_ms               = 0;
    probe_frames_left           = 0;
    probe_recheck_due_ms        = 0;
    probe_failure_logs          = 0;

    /*-----------------------------------------------------*\
    | Read product string                                   |
    \*-----------------------------------------------------*/
    wchar_t product_string[256];

    int ret = hid_get_product_string(dev, product_string, 256);

    if(ret != 0)
    {
        name = "";
    }
    else
    {
        name = StringUtils::wstring_to_string(product_string);
    }

    /*-----------------------------------------------------*\
    | Read vendor string                                    |
    \*-----------------------------------------------------*/
    wchar_t vendor_string[256];

    ret = hid_get_manufacturer_string(dev, vendor_string, 256);

    if(ret != 0)
    {
        vendor = "";
    }
    else
    {
        vendor = StringUtils::wstring_to_string(vendor_string);
    }

    /*-----------------------------------------------------*\
    | Read serial string                                    |
    \*-----------------------------------------------------*/
    wchar_t serial_string[256];

    ret = hid_get_serial_number_string(dev, serial_string, 256);

    if(ret != 0)
    {
        serial = "";
    }
    else
    {
        serial = StringUtils::wstring_to_string(serial_string);
    }

    /*-----------------------------------------------------*\
    | If the device is a Keychron wireless dongle that      |
    | supports wireless RGB control, probe the attached     |
    | keyboard's information                                |
    \*-----------------------------------------------------*/
    CmdGetWirelessDeviceInfo(&wireless_device_vid, &wireless_device_pid);

    if(wireless_device_vid != 0 && wireless_device_pid != 0)
    {
        /*-------------------------------------------------*\
        | Wireless keyboard detected, update dev_pid to use |
        | the keyboard's PID instead of the dongle's PID    |
        \*-------------------------------------------------*/
        dev_pid = wireless_device_pid;
    }

    /*-----------------------------------------------------*\
    | Get VIA protocol version                              |
    \*-----------------------------------------------------*/
    CmdGetViaProtocolVersion(&via_protocol_version);

    /*-----------------------------------------------------*\
    | Get Keychron protocol version                         |
    \*-----------------------------------------------------*/
    CmdGetKeychronProtocolVersion(&kc_protocol_version);

    /*-----------------------------------------------------*\
    | Get Keychron firmware version                         |
    \*-----------------------------------------------------*/
    std::string initial_firmware = CmdGetKeychronFirmwareVersion();

    if(wireless_device_vid != 0 && wireless_device_pid != 0)
    {
        /*-------------------------------------------------*\
        | For wireless connections:                         |
        | - The initial firmware is the dongle's version    |
        | - Query the keyboard's firmware via wireless      |
        \*-------------------------------------------------*/
        kc_dongle_firmware_version = initial_firmware;
        kc_firmware_version = CmdGetWirelessKeyboardFirmwareVersion();
    }
    else
    {
        /*-------------------------------------------------*\
        | For wired connections:                            |
        | - Use the firmware version directly               |
        | - No dongle firmware to display                   |
        \*-------------------------------------------------*/
        kc_firmware_version = initial_firmware;
    }

    /*-----------------------------------------------------*\
    | Get supported Keychron features                       |
    \*-----------------------------------------------------*/
    CmdGetSupportFeature(&supported_features);

    /*-----------------------------------------------------*\
    | Get Keychron RGB protocol version, ahead of the       |
    | GetSupported() gate below since it needs the result   |
    \*-----------------------------------------------------*/
    CmdGetKeychronRGBProtocolVersion(&kc_rgb_protocol_version);

    if(!GetSupported())
    {
        via_reply_timeout = KEYCHRON_QHE_HID_REPLY_TIMEOUT;

        return;
    }

    /*-----------------------------------------------------*\
    | Get count of LEDs                                     |
    \*-----------------------------------------------------*/
    CmdGetNumberLEDs(&number_leds);

    led_info.resize(number_leds);
    keycodes.resize(number_leds);

    for(std::size_t led_idx = 0; led_idx < led_info.size(); led_idx++)
    {
        led_info[led_idx].valid = false;
    }

    /*-----------------------------------------------------*\
    | Get info and keycode for all LEDs                     |
    \*-----------------------------------------------------*/
    for(unsigned char row = 0; row < 32; row++)
    {
        std::vector<unsigned char> row_leds = CmdGetLEDIndexByRow(row);

        for(unsigned char col = 0; col < row_leds.size(); col++)
        {
            if(row_leds[col] != 0xFF && row_leds[col] < led_info.size() && led_info[row_leds[col]].valid == false)
            {
                led_info[row_leds[col]].valid   = true;
                led_info[row_leds[col]].col     = col;
                led_info[row_leds[col]].row     = row;
            }
        }
    }

    for(unsigned short led_index = 0; led_index < number_leds; led_index++)
    {
        if(led_info[led_index].valid)
        {
            keycodes[led_index] = CmdGetKeycode(0, led_info[led_index].row, led_info[led_index].col);
        }
        else
        {
            keycodes[led_index] = 0;
        }
    }

    /*-----------------------------------------------------*\
    | Detection is done, so wait for replies only as long   |
    | as a busy keyboard needs                              |
    \*-----------------------------------------------------*/
    via_reply_timeout = KEYCHRON_QHE_HID_REPLY_TIMEOUT;

    /*-----------------------------------------------------*\
    | Apply matrix corrections                              |
    \*-----------------------------------------------------*/
    const std::vector<qmk_rgb_matrix_led_info> raw_led_info = led_info;

    for(unsigned int patch_idx = 0; patch_idx < KEYCHRON_PATCH_COUNT; patch_idx++)
    {
        if(dev_pid == keychron_patches[patch_idx]->pid)
        {
            for(unsigned int patch_entry_idx = 0;
                patch_entry_idx < keychron_patches[patch_idx]->num_entries;
                patch_entry_idx++)
            {
                const keychron_patch_entry& patch_entry =
                    keychron_patches[patch_idx]->patch[patch_entry_idx];

                for(unsigned int led_idx = 0; led_idx < raw_led_info.size(); led_idx++)
                {
                    if(raw_led_info[led_idx].valid &&
                    raw_led_info[led_idx].row == patch_entry.source_row &&
                    raw_led_info[led_idx].col == patch_entry.source_col)
                    {
                        led_info[led_idx].row = patch_entry.row;
                        led_info[led_idx].col = patch_entry.col;
                        break;
                    }
                }
            }

            break;
        }
    }
}

QMKKeychronController::~QMKKeychronController()
{
    hid_close(dev);
}

std::string QMKKeychronController::GetLocation()
{
    return("HID: " + location);
}

std::string QMKKeychronController::GetName()
{
    return(name);
}

std::string QMKKeychronController::GetSerial()
{
    return(serial);
}

std::string QMKKeychronController::GetVendor()
{
    return(vendor);
}

std::string QMKKeychronController::GetVersion()
{
    /*-----------------------------------------------------*\
    | Format multi-line version text                        |
    \*-----------------------------------------------------*/
    std::string result = "VIA: "          + std::to_string(via_protocol_version) + "\r\n" +
                         "Keychron: "     + std::to_string(kc_protocol_version) + "\r\n" +
                         "Keychron RGB: " + std::to_string(kc_rgb_protocol_version) + "\r\n";

    if(!kc_dongle_firmware_version.empty())
    {
        /*-------------------------------------------------*\
        | Wireless - show both dongle and keyboard          |
        \*-------------------------------------------------*/
        result += "Dongle FW: "   + kc_dongle_firmware_version + "\r\n" +
                  "Keyboard FW: " + kc_firmware_version;
    }
    else
    {
        /*-------------------------------------------------*\
        | Wired - show keyboard only                        |
        \*-------------------------------------------------*/
        result += "Keychron FW: " + kc_firmware_version;
    }

    return(result);
}

bool QMKKeychronController::GetSupported()
{
    /*-----------------------------------------------------*\
    | Some boards don't set the feature bit even though the |
    | RGB sub-protocol responds, so fall back to that       |
    \*-----------------------------------------------------*/
    return((supported_features & KC_FEATURE_KEYCHRON_RGB) || (kc_rgb_protocol_version != 0));
}

unsigned short QMKKeychronController::GetKeycode(unsigned short led_index)
{
    return(keycodes[led_index]);
}

unsigned short QMKKeychronController::GetLEDCount()
{
    return(number_leds);
}

qmk_rgb_matrix_led_info QMKKeychronController::GetLEDInfo(unsigned short led_index)
{
    return(led_info[led_index]);
}

void QMKKeychronController::SaveMode()
{
    CmdSaveMode();
}

void QMKKeychronController::SendLEDs(unsigned short number_leds, RGBColor* color_data)
{
    unsigned short      led_start_index     = 0;
    unsigned short      number_packet_leds  = 9;
    unsigned char       frame_brightness    = 0;

    /*-----------------------------------------------------*\
    | One-shot resend a few seconds after the first Direct  |
    | mode switch: replay the full mode sequence, then fall |
    | through to send the complete frame below. Runs on the |
    | calling thread, so there is no background work to     |
    | outlive the controller.                               |
    \*-----------------------------------------------------*/
    if(direct_mode && resend_armed && !resend_done && !resend_active && (KeychronNowMs() >= resend_due_ms))
    {
        resend_done   = true;
        resend_active = true;

        LOG_WARNING("%s one-shot resend: replaying SetMode and a full frame", KEYCHRON_PROBE_PREFIX);

        ApplyMode(saved_mode, saved_speed, saved_hue, saved_sat, saved_val);

        resend_active = false;
    }

    bool                probing             = (direct_mode && (probe_frames_left > 0));
    int                 brightness_result   = -2;

    if(direct_mode && (probe_recheck_due_ms != 0) && (KeychronNowMs() >= probe_recheck_due_ms))
    {
        probe_recheck_due_ms = 0;
        ProbeReadState("recheck 3s after first frame");
    }

    /*-----------------------------------------------------*\
    | The per-key effect ignores each key's value and shows |
    | every key at the matrix brightness, so follow the     |
    | brightest LED in the frame, unsaved. Hardware modes   |
    | keep their own brightness.                            |
    \*-----------------------------------------------------*/
    for(unsigned short led_idx = 0; direct_mode && (led_idx < number_leds); led_idx++)
    {
        unsigned char led_max = RGBGetRValue(color_data[led_idx]);

        if(RGBGetGValue(color_data[led_idx]) > led_max)
        {
            led_max = RGBGetGValue(color_data[led_idx]);
        }

        if(RGBGetBValue(color_data[led_idx]) > led_max)
        {
            led_max = RGBGetBValue(color_data[led_idx]);
        }

        if(led_max > frame_brightness)
        {
            frame_brightness = led_max;
        }
    }

    if(direct_mode && (last_frame_brightness != frame_brightness))
    {
        bool brightness_ok = CmdSetBrightness(frame_brightness);

        brightness_result  = brightness_ok ? 1 : 0;

        if(brightness_ok)
        {
            last_frame_brightness = frame_brightness;
        }
    }

    if(direct_mode && (frame_brightness == 0))
    {
        if(probing)
        {
            LOG_WARNING("%s frame skipped: peak brightness 0 (brightness set result %d)", KEYCHRON_PROBE_PREFIX, brightness_result);
            probe_frames_left--;
        }

        return;
    }

    unsigned int        packets_sent        = 0;
    unsigned int        packets_acked       = 0;
    char                reply_text[256]     = { 0 };
    size_t              reply_len           = 0;

    while(led_start_index < number_leds)
    {
        if((number_leds - led_start_index) < 9)
        {
            number_packet_leds = (number_leds - led_start_index);
        }

        unsigned char reply[4] = { 0 };
        int           result   = CmdSendLEDs((unsigned char)led_start_index, (unsigned char)number_packet_leds, &color_data[led_start_index], reply);

        packets_sent++;

        if(result >= 0)
        {
            packets_acked++;
        }

        if(probing && (reply_len + 16 < sizeof(reply_text)))
        {
            reply_len += snprintf(&reply_text[reply_len], sizeof(reply_text) - reply_len, " [%d:%02X%02X%02X%02X]", result, reply[0], reply[1], reply[2], reply[3]);
        }

        led_start_index += number_packet_leds;
    }

    if(probing)
    {
        if(probe_keys.empty())
        {
            const char key_chars[4]  = { 'W', 'A', 'S', 'D' };
            const unsigned short key_codes[4] = { QMK_KC_W, QMK_KC_A, QMK_KC_S, QMK_KC_D };

            for(unsigned int key = 0; key < 4; key++)
            {
                for(unsigned short led_idx = 0; led_idx < keycodes.size(); led_idx++)
                {
                    if(keycodes[led_idx] == key_codes[key])
                    {
                        probe_keys.push_back(std::make_pair(key_chars[key], led_idx));
                        break;
                    }
                }
            }
        }

        char key_text[200] = { 0 };
        size_t key_len     = 0;

        for(size_t key = 0; key < probe_keys.size(); key++)
        {
            unsigned short led_idx = probe_keys[key].second;

            if(led_idx < number_leds)
            {
                hsv_t hsv_color;
                rgb2hsv(color_data[led_idx], &hsv_color);

                key_len += snprintf(&key_text[key_len], sizeof(key_text) - key_len, " %c(led %u)=h%u s%u v%u", probe_keys[key].first, (unsigned int)led_idx,
                                    (unsigned int)((float)hsv_color.hue * (256.0f / 360.0f)), (unsigned int)hsv_color.saturation, (unsigned int)hsv_color.value);
            }
        }

        LOG_WARNING("%s frame (%d left): leds=%u peak=%u brightness set result %d, packets %u acked %u of %u;%s; replies [ret:b0b1b2b3]%s",
                    KEYCHRON_PROBE_PREFIX, probe_frames_left, (unsigned int)number_leds, (unsigned int)frame_brightness, brightness_result,
                    packets_sent, packets_acked, packets_sent, key_text, reply_text);

        if((probe_frames_left == KEYCHRON_PROBE_FRAMES) && (probe_recheck_due_ms == 0))
        {
            probe_recheck_due_ms = KeychronNowMs() + KEYCHRON_PROBE_RECHECK_MS;
        }

        probe_frames_left--;
    }
}

void QMKKeychronController::SetMode(unsigned short mode, unsigned char speed, unsigned char hue, unsigned char sat, unsigned char val)
{
    saved_mode  = mode;
    saved_speed = speed;
    saved_hue   = hue;
    saved_sat   = sat;
    saved_val   = val;

    ApplyMode(mode, speed, hue, sat, val);

    /*-----------------------------------------------------*\
    | Arm the one-shot resend on the first Direct switch    |
    \*-----------------------------------------------------*/
    if((mode == 0xFFFF) && !resend_armed)
    {
        resend_armed  = true;
        resend_due_ms = KeychronNowMs() + KEYCHRON_RESEND_DELAY_MS;
    }
}

void QMKKeychronController::ApplyMode(unsigned short mode, unsigned char speed, unsigned char hue, unsigned char sat, unsigned char val)
{
    /*-----------------------------------------------------*\
    | The next frame must resend its brightness             |
    \*-----------------------------------------------------*/
    last_frame_brightness = -1;
    direct_mode           = (mode == 0xFFFF);
    probe_frames_left     = KEYCHRON_PROBE_FRAMES;
    probe_recheck_due_ms  = 0;

    LOG_WARNING("%s SetMode enter: mode %u speed %u hue %u sat %u val %u, cached per-key effect %d, resend %s", KEYCHRON_PROBE_PREFIX,
                (unsigned int)mode, (unsigned int)speed, (unsigned int)hue, (unsigned int)sat, (unsigned int)val, per_key_effect,
                resend_active ? "pass" : "initial");

    if(mode == 0xFFFF)
    {
        /*-------------------------------------------------*\
        | The per-key effect ignores each key's value and   |
        | uses the matrix brightness, which the firmware    |
        | reloads from EEPROM when the effect changes, so   |
        | save brightness before switching effects. The     |
        | effect itself is left unsaved.                    |
        |                                                   |
        | The save writes only when exactly one value was   |
        | set since the last save and always discards the   |
        | set values, so the first save drops stale ones.   |
        | Saving only after a brightness write that landed  |
        | keeps the effect out of EEPROM.                   |
        \*-------------------------------------------------*/
        bool save_one = CmdSetBrightness(KEYCHRON_QHE_MAX_BRIGHTNESS);

        if(save_one)
        {
            CmdSaveRGBMatrix();
        }

        bool save_two = CmdSetBrightness(KEYCHRON_QHE_MAX_BRIGHTNESS);

        if(save_two)
        {
            CmdSaveRGBMatrix();
        }

        unsigned char effect = GetPerKeyEffect();

        CmdSetRGBMatrixMode(effect);
        CmdSetPerKeyRGBType(KEYCHRON_PER_KEY_RGB_SOLID);

        LOG_WARNING("%s SetMode direct: brightness set before save 1 %s, before save 2 %s, per-key effect id %u", KEYCHRON_PROBE_PREFIX,
                    save_one ? "true" : "false", save_two ? "true" : "false", (unsigned int)effect);
    }
    else
    {
        CmdSetRGBMatrixMode((unsigned char)mode);
        CmdSetColorHS(hue, sat);
        CmdSetBrightness(val);
        CmdSetSpeed(speed);
    }

    ProbeReadState("SetMode exit");
}

unsigned char QMKKeychronController::GetPerKeyEffect()
{
    /*-----------------------------------------------------*\
    | The effect id depends on how many animations the      |
    | firmware enables. The custom effects come last, Per   |
    | Key RGB then Mix RGB, and the firmware clamps an      |
    | out of range mode to the last effect, so asking for   |
    | the highest id reveals Mix RGB and Per Key RGB is the |
    | one before it. The switch is not saved.               |
    \*-----------------------------------------------------*/
    if(per_key_effect == 0)
    {
        /*-------------------------------------------------*\
        | A firmware that ignores the probe leaves the      |
        | current effect in place, so a readback equal to   |
        | the prior mode, or a prior read that failed, is   |
        | probed again from a known mode.                   |
        \*-------------------------------------------------*/
        int prior_effect = CmdGetRGBMatrixMode();

        CmdSetRGBMatrixMode(0xFF);

        int last_effect = CmdGetRGBMatrixMode();

        LOG_WARNING("%s effect probe: prior effect %d, read-back after 0xFF %d", KEYCHRON_PROBE_PREFIX, prior_effect, last_effect);

        if((prior_effect < 0) || (last_effect == prior_effect))
        {
            CmdSetRGBMatrixMode(1);
            CmdSetRGBMatrixMode(0xFF);

            last_effect = CmdGetRGBMatrixMode();

            LOG_WARNING("%s effect probe retry from mode 1: read-back %d", KEYCHRON_PROBE_PREFIX, last_effect);

            if(last_effect == 1)
            {
                LOG_WARNING("%s effect probe: firmware ignored probe, using fallback id %d", KEYCHRON_PROBE_PREFIX, KEYCHRON_QHE_PER_KEY_RGB_EFFECT);

                return(KEYCHRON_QHE_PER_KEY_RGB_EFFECT);
            }
        }

        if((last_effect > 2) && (last_effect < 0xFF))
        {
            per_key_effect = last_effect - 1;

            LOG_WARNING("%s effect probe: chosen per-key effect id %d (cached)", KEYCHRON_PROBE_PREFIX, per_key_effect);
        }
        else
        {
            LOG_WARNING("%s effect probe: read-back %d unusable, using fallback id %d", KEYCHRON_PROBE_PREFIX, last_effect, KEYCHRON_QHE_PER_KEY_RGB_EFFECT);

            return(KEYCHRON_QHE_PER_KEY_RGB_EFFECT);
        }
    }

    return((unsigned char)per_key_effect);
}

void QMKKeychronController::LogReplyWarning(const char* format, ...)
{
    /*-----------------------------------------------------*\
    | Log the first occurrence, then at most once per       |
    | warning interval with the number of warnings held back|
    \*-----------------------------------------------------*/
    long long now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

    if((last_warning_ms != 0) && ((now_ms - last_warning_ms) < KEYCHRON_QHE_WARNING_INTERVAL_MS))
    {
        suppressed_warnings++;
        return;
    }

    char    message[160];
    va_list args;

    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    if(suppressed_warnings > 0)
    {
        LOG_WARNING("[QMK Keychron] %s (%u more since the last report)", message, suppressed_warnings);
    }
    else
    {
        LOG_WARNING("[QMK Keychron] %s", message);
    }

    last_warning_ms     = now_ms;
    suppressed_warnings = 0;
}

void QMKKeychronController::ProbeReadState(const char* when)
{
    unsigned char type_reply[4] = { 0 };

    int effect_now     = CmdGetRGBMatrixMode();
    int brightness_now = CmdGetBrightness();
    int type_result    = CmdGetPerKeyRGBType(type_reply);

    LOG_WARNING("%s state (%s): effect %d, brightness %d, per-key type reply %d [%02X %02X %02X %02X]", KEYCHRON_PROBE_PREFIX, when,
                effect_now, brightness_now, type_result, type_reply[0], type_reply[1], type_reply[2], type_reply[3]);
}

void QMKKeychronController::ProbeLogFailure(const char* what, unsigned char cmd, int subcmd)
{
    if(probe_failure_logs >= KEYCHRON_PROBE_MAX_FAILURE_LOGS)
    {
        return;
    }

    probe_failure_logs++;

    if(subcmd >= 0)
    {
        LOG_WARNING("%s raw command %02X %02X: %s", KEYCHRON_PROBE_PREFIX, cmd, (unsigned int)subcmd, what);
    }
    else
    {
        LOG_WARNING("%s raw command %02X: %s", KEYCHRON_PROBE_PREFIX, cmd, what);
    }
}

unsigned short QMKKeychronController::CmdGetKeycode
    (
    unsigned char       layer,
    unsigned char       row,
    unsigned char       col
    )
{
    unsigned char       args[3];
    unsigned char       response[5] = { 0 };

    args[0] = layer;
    args[1] = row;
    args[2] = col;

    if(ViaSendCommand(QMK_VIA_CMD_VIA_DYNAMIC_KEYMAP_GET_KEYCODE, args, sizeof(args), response, sizeof(response)) <= 0)
    {
        return(0);
    }

    return((response[3] << 8) | response[4]);
}


std::string QMKKeychronController::CmdGetKeychronFirmwareVersion()
{
    char                response[30] = { 0 };

    if(ViaSendCommand(KC_GET_FIRMWARE_VERSION, NULL, 0, (unsigned char*)response, sizeof(response)) <= 0)
    {
        return("");
    }

    /*-----------------------------------------------------*\
    | Ensure response null termination                      |
    \*-----------------------------------------------------*/
    response[29] = 0;

    return(std::string(response));
}

void QMKKeychronController::CmdGetKeychronProtocolVersion
    (
    unsigned char*      kc_protocol_version
    )
{
    *kc_protocol_version = 0;

    ViaSendCommand(KC_GET_PROTOCOL_VERSION, NULL, 0, (unsigned char*)kc_protocol_version, sizeof(unsigned char));
}

void QMKKeychronController::CmdGetKeychronRGBProtocolVersion(unsigned short* kc_rgb_protocol_version)
{
    *kc_rgb_protocol_version = 0;

    if(ViaSendCommandSub(KC_KEYCHRON_RGB, KEYCHRON_RGB_PROTOCOL_VER, NULL, 0, (unsigned char*)kc_rgb_protocol_version, sizeof(unsigned short)) <= 0)
    {
        return;
    }

    /*-----------------------------------------------------*\
    | The RGB protocol version byte order is reversed       |
    \*-----------------------------------------------------*/
    *kc_rgb_protocol_version = ((*kc_rgb_protocol_version & 0x00FF) << 8) | ((*kc_rgb_protocol_version & 0xFF00) >> 8);
}

std::vector<unsigned char> QMKKeychronController::CmdGetLEDIndexByRow(unsigned char row)
{
    unsigned char args[4];
    unsigned char response[KEYCHRON_QHE_PACKET_SIZE - 2];

    args[0] = row;
    args[1] = 0xFF;
    args[2] = 0xFF;
    args[3] = 0xFF;

    int bytes_read = ViaSendCommandSub(KC_KEYCHRON_RGB, KEYCHRON_RGB_LED_IDX, args, sizeof(args), response, sizeof(response));

    std::vector<unsigned char> result;

    if(bytes_read > 0)
    {
        for(int i = 1; i < bytes_read; i++)
        {
            result.push_back(response[i] == 0xFF ? -1 : response[i]);
        }
    }

    return result;
}

void QMKKeychronController::CmdGetNumberLEDs
    (
    unsigned short*     number_leds
    )
{
    *number_leds = 0;

    if(ViaSendCommandSub(KC_KEYCHRON_RGB, KEYCHRON_RGB_LED_COUNT, NULL, 0, (unsigned char*)number_leds, sizeof(unsigned short)) <= 0)
    {
        return;
    }

    /*-----------------------------------------------------*\
    | The LED count byte order is reversed                  |
    \*-----------------------------------------------------*/
    *number_leds = ((*number_leds & 0x00FF) << 8) | ((*number_leds & 0xFF00) >> 8);
}

void QMKKeychronController::CmdGetSupportFeature(unsigned short* supported_features)
{
    unsigned char response[3] = { 0 };

    *supported_features = 0;

    if(ViaSendCommand(KC_GET_SUPPORT_FEATURE, NULL, 0, response, sizeof(response)) <= 0)
    {
        return;
    }

    /*-----------------------------------------------------*\
    | The QMK and ZMK variants of this packet differ by one |
    | byte position                                         |
    \*-----------------------------------------------------*/
    if(response[0] == 0)
    {
        *supported_features = (response[2] << 8) | response[1];
    }
    else
    {
        *supported_features = (response[1] << 8) | response[0];
    }
}

void QMKKeychronController::CmdGetViaProtocolVersion
    (
    unsigned short*     via_protocol_version
    )
{
    *via_protocol_version = 0;

    if(ViaSendCommand(QMK_VIA_CMD_GET_PROTOCOL_VERSION, NULL, 0, (unsigned char*)via_protocol_version, sizeof(unsigned short)) <= 0)
    {
        return;
    }

    /*-----------------------------------------------------*\
    | The protocol version byte order is reversed           |
    \*-----------------------------------------------------*/
    *via_protocol_version = ((*via_protocol_version & 0x00FF) << 8) | ((*via_protocol_version & 0xFF00) >> 8);
}

void QMKKeychronController::CmdGetWirelessDeviceInfo
    (
    unsigned short*     wireless_vid,
    unsigned short*     wireless_pid
    )
{
    /*-----------------------------------------------------*\
    | Query wireless keyboard information via dongle        |
    | This allows the controller to identify the actual     |
    | keyboard's USB VID/PID instead of the dongle's        |
    \*-----------------------------------------------------*/
    unsigned char response[10] = { 0 };

    *wireless_vid = 0;
    *wireless_pid = 0;

    if(ViaSendCommand(KC_WIRELESS_DEVICE_INFO, NULL, 0, response, sizeof(response)) <= 0)
    {
        return;
    }

    /*-----------------------------------------------------*\
    | Response format:                                      |
    | Byte 0: Status (0x01 = connected/valid)               |
    | Bytes 1-2: VID (little-endian)                        |
    | Bytes 3-4: PID (little-endian)                        |
    | Bytes 5+: Additional info                             |
    \*-----------------------------------------------------*/
    if(response[0] != 0x01)
    {
        /*-------------------------------------------------*\
        | No wireless keyboard connected                    |
        \*-------------------------------------------------*/
        return;
    }

    *wireless_vid = response[1] | (response[2] << 8);
    *wireless_pid = response[3] | (response[4] << 8);
}

std::string QMKKeychronController::CmdGetWirelessKeyboardFirmwareVersion()
{
    /*-----------------------------------------------------*\
    | Query wireless keyboard firmware version via dongle   |
    | Similar to CmdGetKeychronFirmwareVersion but for      |
    | the connected wireless keyboard instead of the dongle |
    \*-----------------------------------------------------*/
    char response[30] = { 0 };

    if(ViaSendCommand(KC_WIRELESS_FIRMWARE_VERSION, NULL, 0, (unsigned char*)response, sizeof(response)) <= 0)
    {
        return("");
    }

    /*-----------------------------------------------------*\
    | Ensure response null termination                      |
    \*-----------------------------------------------------*/
    response[29] = 0;

    return(std::string(response));
}

void QMKKeychronController::CmdSaveMode()
{
    ViaSendCommandSub(KC_KEYCHRON_RGB, KEYCHRON_RGB_SAVE, NULL, 0, NULL, 0);
}

int QMKKeychronController::CmdGetRGBMatrixMode()
{
    unsigned char args[2];
    unsigned char response[3] = { 0 };

    args[0] = QMK_VIA_RGB_MATRIX_CHANNEL;
    args[1] = QMK_VIA_RGB_MATRIX_EFFECT;

    if(ViaSendCommand(QMK_VIA_CMD_CUSTOM_GET_VALUE, args, sizeof(args), response, sizeof(response)) <= 0)
    {
        return(-1);
    }

    return(response[2]);
}

int QMKKeychronController::CmdGetBrightness()
{
    unsigned char args[2];
    unsigned char response[3] = { 0 };

    args[0] = QMK_VIA_RGB_MATRIX_CHANNEL;
    args[1] = QMK_VIA_RGB_MATRIX_BRIGHTNESS;

    if(ViaSendCommand(QMK_VIA_CMD_CUSTOM_GET_VALUE, args, sizeof(args), response, sizeof(response)) <= 0)
    {
        return(-1);
    }

    return(response[2]);
}

int QMKKeychronController::CmdGetPerKeyRGBType(unsigned char* reply)
{
    return(ViaSendCommandSub(KC_KEYCHRON_RGB, KEYCHRON_RGB_PER_KEY_GET_TYPE, NULL, 0, reply, 4));
}

void QMKKeychronController::CmdSaveRGBMatrix()
{
    unsigned char args[1];

    args[0] = QMK_VIA_RGB_MATRIX_CHANNEL;

    ViaSendCommand(QMK_VIA_CMD_CUSTOM_SAVE, args, sizeof(args), NULL, 0);
}

int QMKKeychronController::CmdSendLEDs(unsigned char start_index, unsigned char number_leds, RGBColor* color_data, unsigned char* reply)
{
    unsigned char args[KEYCHRON_QHE_PACKET_SIZE - 2];

    memset(args, 0, sizeof(args));

    args[0]         = start_index;
    args[1]         = number_leds;

    if(number_leds > 9)
    {
        number_leds = 9;
    }

    for(unsigned char led_index = 0; led_index < number_leds; led_index++)
    {
        /*-------------------------------------------------*\
        | VialRGB sends direct packets in HSV for some      |
        | inexplicable reason, so do the RGB to HSV         |
        | conversion before sending                         |
        \*-------------------------------------------------*/
        hsv_t hsv_color;
        rgb2hsv(color_data[led_index], &hsv_color);

        args[2 + (led_index * 3)]   = (unsigned char)((float)hsv_color.hue * (256.0f / 360.0f));
        args[3 + (led_index * 3)]   = hsv_color.saturation;
        args[4 + (led_index * 3)]   = hsv_color.value;
    }

    return(ViaSendCommandSub(KC_KEYCHRON_RGB, KEYCHRON_RGB_PER_KEY_SET_COLOR, args, sizeof(args), reply, 4));
}

bool QMKKeychronController::CmdSetBrightness(unsigned char brightness)
{
    unsigned char args[3];

    args[0] = QMK_VIA_RGB_MATRIX_CHANNEL;
    args[1] = QMK_VIA_RGB_MATRIX_BRIGHTNESS;
    args[2] = brightness;

    return(ViaSendCommand(QMK_VIA_CMD_CUSTOM_SET_VALUE, args, sizeof(args), NULL, 0) >= 0);
}

void QMKKeychronController::CmdSetColorHS(unsigned char h, unsigned char s)
{
    unsigned char args[4];

    args[0] = QMK_VIA_RGB_MATRIX_CHANNEL;
    args[1] = QMK_VIA_RGB_MATRIX_COLOR;
    args[2] = h;
    args[3] = s;

    ViaSendCommand(QMK_VIA_CMD_CUSTOM_SET_VALUE, args, sizeof(args), NULL, 0);
}

void QMKKeychronController::CmdSetPerKeyRGBType(unsigned char type)
{
    unsigned char args[1];

    args[0] = type;

    ViaSendCommandSub(KC_KEYCHRON_RGB, KEYCHRON_RGB_PER_KEY_SET_TYPE, args, sizeof(args), NULL, 0);
}

void QMKKeychronController::CmdSetRGBMatrixMode(unsigned char mode)
{
    unsigned char args[3];

    args[0] = QMK_VIA_RGB_MATRIX_CHANNEL;
    args[1] = QMK_VIA_RGB_MATRIX_EFFECT;
    args[2] = mode;

    ViaSendCommand(QMK_VIA_CMD_CUSTOM_SET_VALUE, args, sizeof(args), NULL, 0);
}

void QMKKeychronController::CmdSetSpeed(unsigned char speed)
{
    unsigned char args[3];

    args[0] = QMK_VIA_RGB_MATRIX_CHANNEL;
    args[1] = QMK_VIA_RGB_MATRIX_EFFECT_SPEED;
    args[2] = speed;

    ViaSendCommand(QMK_VIA_CMD_CUSTOM_SET_VALUE, args, sizeof(args), NULL, 0);
}

int QMKKeychronController::ViaSendCommand
    (
    unsigned char       cmd,
    unsigned char*      data_in,
    unsigned char       data_in_size,
    unsigned char*      data_out,
    unsigned char       data_out_size
    )
{
    /*-----------------------------------------------------*\
    | Standard VIA command with no sub-command              |
    |                                                       |
    | Byte 0: Command                                       |
    | Byte 1+: Data                                         |
    \*-----------------------------------------------------*/
    unsigned char usb_buf[KEYCHRON_QHE_PACKET_SIZE + 1];

    memset(usb_buf, 0, sizeof(usb_buf));

    /*-----------------------------------------------------*\
    | Write command, offsetting by 1 for HID report ID      |
    \*-----------------------------------------------------*/
    usb_buf[1] = cmd;
    memcpy(&usb_buf[2], data_in, data_in_size);

    /*-----------------------------------------------------*\
    | Drop reports queued since the last command, such as   |
    | the layer change notifications the firmware sends on  |
    | its own                                               |
    \*-----------------------------------------------------*/
    unsigned char drain_buf[KEYCHRON_QHE_PACKET_SIZE + 1];

    for(unsigned char drain_count = 0; drain_count < 64; drain_count++)
    {
        if(hid_read_timeout(dev, drain_buf, sizeof(drain_buf) - 1, 0) <= 0)
        {
            break;
        }
    }

    if(hid_write(dev, usb_buf, sizeof(usb_buf)) < 0)
    {
        ProbeLogFailure("hid_write failed", cmd, -1);

        return(-1);
    }

    /*-----------------------------------------------------*\
    | Read until the reply to this command arrives. The     |
    | custom value commands echo the channel and value ID.  |
    \*-----------------------------------------------------*/
    std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(via_reply_timeout);

    unsigned char discarded[4] = { 0 };
    bool          have_discard = false;

    while(std::chrono::steady_clock::now() < deadline)
    {
        int remaining = (int)std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
        int bytes_received = hid_read_timeout(dev, usb_buf, sizeof(usb_buf) - 1, (remaining > 0) ? remaining : 0);

        if(bytes_received < 0)
        {
            ProbeLogFailure("hid_read failed", cmd, -1);

            return(-1);
        }

        if(bytes_received == 0)
        {
            continue;
        }

        if(usb_buf[0] != cmd
        || ((cmd == QMK_VIA_CMD_CUSTOM_SET_VALUE || cmd == QMK_VIA_CMD_CUSTOM_GET_VALUE) && data_in_size >= 2 && (usb_buf[1] != data_in[0] || usb_buf[2] != data_in[1]))
        || (cmd == QMK_VIA_CMD_CUSTOM_SAVE && data_in_size >= 1 && usb_buf[1] != data_in[0]))
        {
            if(!have_discard)
            {
                memcpy(discarded, usb_buf, sizeof(discarded));
                have_discard = true;
            }

            if(usb_buf[0] == 0xFF)
            {
                ProbeLogFailure("reported unhandled (reply cmd FF)", cmd, -1);
                LogReplyWarning("Command %02X was reported unhandled", cmd);

                return(-1);
            }

            continue;
        }

        if(have_discard)
        {
            LogReplyWarning("Command %02X got a mismatched reply first: %02X %02X %02X %02X", cmd, discarded[0], discarded[1], discarded[2], discarded[3]);
        }

        memcpy(data_out, &usb_buf[1], data_out_size);

        return(bytes_received - 1);
    }

    ProbeLogFailure("timed out", cmd, -1);

    if(have_discard)
    {
        LogReplyWarning("Command %02X timed out, first discarded reply: %02X %02X %02X %02X", cmd, discarded[0], discarded[1], discarded[2], discarded[3]);
    }
    else
    {
        LogReplyWarning("Command %02X timed out", cmd);
    }

    return(-1);
}

int QMKKeychronController::ViaSendCommandSub
    (
    unsigned char       cmd,
    unsigned char       subcmd,
    unsigned char*      data_in,
    unsigned char       data_in_size,
    unsigned char*      data_out,
    unsigned char       data_out_size
    )
{
    /*-----------------------------------------------------*\
    | Standard VIA command with sub-command                 |
    |                                                       |
    | Byte 0: Command                                       |
    | Byte 1: Sub-Command                                   |
    | Byte 2+: Data                                         |
    \*-----------------------------------------------------*/
    unsigned char usb_buf[KEYCHRON_QHE_PACKET_SIZE + 1];

    memset(usb_buf, 0, sizeof(usb_buf));

    /*-----------------------------------------------------*\
    | Write command, offsetting by 1 for HID report ID      |
    \*-----------------------------------------------------*/
    usb_buf[1] = cmd;
    usb_buf[2] = subcmd;
    memcpy(&usb_buf[3], data_in, data_in_size);

    /*-----------------------------------------------------*\
    | Drop reports queued since the last command, such as   |
    | the layer change notifications the firmware sends on  |
    | its own                                               |
    \*-----------------------------------------------------*/
    unsigned char drain_buf[KEYCHRON_QHE_PACKET_SIZE + 1];

    for(unsigned char drain_count = 0; drain_count < 64; drain_count++)
    {
        if(hid_read_timeout(dev, drain_buf, sizeof(drain_buf) - 1, 0) <= 0)
        {
            break;
        }
    }

    if(hid_write(dev, usb_buf, sizeof(usb_buf)) < 0)
    {
        ProbeLogFailure("hid_write failed", cmd, subcmd);

        return(-1);
    }

    /*-----------------------------------------------------*\
    | Read until the reply to this command arrives          |
    \*-----------------------------------------------------*/
    std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(via_reply_timeout);

    unsigned char discarded[4] = { 0 };
    bool          have_discard = false;

    while(std::chrono::steady_clock::now() < deadline)
    {
        int remaining = (int)std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
        int bytes_received = hid_read_timeout(dev, usb_buf, sizeof(usb_buf) - 1, (remaining > 0) ? remaining : 0);

        if(bytes_received < 0)
        {
            ProbeLogFailure("hid_read failed", cmd, subcmd);

            return(-1);
        }

        if(bytes_received == 0)
        {
            continue;
        }

        if(usb_buf[0] != cmd || usb_buf[1] != subcmd)
        {
            if(!have_discard)
            {
                memcpy(discarded, usb_buf, sizeof(discarded));
                have_discard = true;
            }

            if(usb_buf[0] == 0xFF)
            {
                ProbeLogFailure("reported unhandled (reply cmd FF)", cmd, subcmd);
                LogReplyWarning("Command %02X %02X was reported unhandled", cmd, subcmd);

                return(-1);
            }

            continue;
        }

        if(have_discard)
        {
            LogReplyWarning("Command %02X %02X got a mismatched reply first: %02X %02X %02X %02X", cmd, subcmd, discarded[0], discarded[1], discarded[2], discarded[3]);
        }

        memcpy(data_out, &usb_buf[2], data_out_size);

        return(bytes_received - 2);
    }

    ProbeLogFailure("timed out", cmd, subcmd);

    if(have_discard)
    {
        LogReplyWarning("Command %02X %02X timed out, first discarded reply: %02X %02X %02X %02X", cmd, subcmd, discarded[0], discarded[1], discarded[2], discarded[3]);
    }
    else
    {
        LogReplyWarning("Command %02X %02X timed out", cmd, subcmd);
    }

    return(-1);
}
