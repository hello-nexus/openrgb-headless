/*---------------------------------------------------------*\
| AsusAuraMainboardController.cpp                           |
|                                                           |
|   Driver for ASUS Aura mainboard                          |
|                                                           |
|   Martin Hartl (inlart)                       25 Apr 2020 |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <cstdio>
#include <cstring>
#include "AsusAuraMainboardController.h"

AuraMainboardController::AuraMainboardController(hid_device* dev_handle, const char* path, std::string dev_name) : AuraUSBController(dev_handle, path, dev_name), mode(AURA_MODE_DIRECT)
{
    unsigned char num_total_mainboard_leds  = config_table[0x1B];
    unsigned char num_rgb_headers           = config_table[0x1D];
    unsigned char num_addressable_headers   = config_table[0x02];
    unsigned char effect_channel            = 0;
    bool          has_gen2_header           = false;

    if(num_total_mainboard_leds < num_rgb_headers)
    {
        num_rgb_headers = 0;
    }

    /*-----------------------------------------------------*\
    | Add mainboard device                                  |
    \*-----------------------------------------------------*/
    if(num_total_mainboard_leds > 0)
    {
        device_info.push_back({effect_channel, 0x04, num_total_mainboard_leds, num_rgb_headers, AuraDeviceType::FIXED});
        effect_channel++;
    }

    /*-----------------------------------------------------*\
    | Add addressable devices                               |
    \*-----------------------------------------------------*/
    for(int i = 0; i < num_addressable_headers; i++)
    {
        /*-----------------------------------------------------*\
        | A header the config table marks Gen2 takes effect     |
        | channel 0x10 + header and subchannel 1 in its direct  |
        | packets                                               |
        \*-----------------------------------------------------*/
        if(i < AURA_CONFIG_HEADER_ENTRIES && config_table[0x04 + (i * 6)] == AURA_HEADER_PROTOCOL_GEN2)
        {
            device_info.push_back({(unsigned char)(AURA_GEN2_EFFECT_CHANNEL + i), (unsigned char)i, 0x01, 0, AuraDeviceType::ADDRESSABLE, 0x01});
            has_gen2_header = true;
        }
        else
        {
            device_info.push_back({effect_channel, (unsigned char)i, 0x01, 0, AuraDeviceType::ADDRESSABLE});
        }
        effect_channel++;
    }

    std::string table;
    char        hex[4];

    for(int i = 0; i < 60; i++)
    {
        snprintf(hex, sizeof(hex), "%02X ", config_table[i]);
        table += hex;
    }

    LOG_WARNING("[%s] ASUS Aura USB firmware %.16s, Gen2 headers %s, config table: %s", name.c_str(), version, has_gen2_header ? "yes" : "no", table.c_str());

    SetGen1();

    if(has_gen2_header)
    {
        SendArgbEnable();
    }
}

AuraMainboardController::~AuraMainboardController()
{
}

void AuraMainboardController::SetGen1()
{
    unsigned char usb_buf[65];

    /*-----------------------------------------------------*\
    | Zero out buffer                                       |
    \*-----------------------------------------------------*/
    memset(usb_buf, 0x00, sizeof(usb_buf));

    /*-----------------------------------------------------*\
    | Set up custom command packet                          |
    \*-----------------------------------------------------*/
    usb_buf[0x00] = 0xEC;
    usb_buf[0x01] = 0x52;
    usb_buf[0x02] = 0x53;
    usb_buf[0x03] = 0x00;
    usb_buf[0x04] = 0x01;

    /*-----------------------------------------------------*\
    | Send packet                                           |
    \*-----------------------------------------------------*/
    hid_write(dev, usb_buf, 65);
}

void AuraMainboardController::SetChannelLEDs(unsigned char channel, RGBColor * colors, unsigned int num_colors)
{
    SendDirect
    (
        device_info[channel].direct_channel,
        num_colors,
        colors,
        device_info[channel].subchannel
    );

}

void AuraMainboardController::SetMode
    (
    unsigned char   channel,
    unsigned char   mode,
    unsigned char   red,
    unsigned char   grn,
    unsigned char   blu
    )
{
    SetMode(channel, mode, red, grn, blu, false);
}

void AuraMainboardController::SetMode
    (
    unsigned char   channel,
    unsigned char   mode,
    unsigned char   red,
    unsigned char   grn,
    unsigned char   blu,
    bool            shutdown_effect
    )
{
    this->mode = mode;
    RGBColor color = ToRGBColor(red, grn, blu);

    SendEffect(device_info[channel].effect_channel, mode, shutdown_effect);
    if(mode == AURA_MODE_DIRECT)
    {
        return;
    }

    unsigned char   led_data[60];
    unsigned char   start_led = 0;

    for(std::size_t i = 0; i < channel; ++i)
    {
        start_led += device_info[i].num_leds;
    }

    for(std::size_t led_idx = 0; led_idx < device_info[channel].num_leds; led_idx++)
    {
        led_data[(led_idx * 3) + 0] = RGBGetRValue(color);
        led_data[(led_idx * 3) + 1] = RGBGetGValue(color);
        led_data[(led_idx * 3) + 2] = RGBGetBValue(color);
    }

    SendColor
    (
        channel,
        start_led,
        device_info[channel].num_leds,
        led_data,
        shutdown_effect
    );
}

void AuraMainboardController::SendArgbEnable()
{
    unsigned char usb_buf[65];

    /*-----------------------------------------------------*\
    | Zero out buffer                                       |
    \*-----------------------------------------------------*/
    memset(usb_buf, 0x00, sizeof(usb_buf));

    /*-----------------------------------------------------*\
    | Set up message packet, sent as 01 01 then 00 01       |
    \*-----------------------------------------------------*/
    usb_buf[0x00]   = 0xEC;
    usb_buf[0x01]   = AURA_MAINBOARD_CONTROL_MODE_ARGB_ENABLE;
    usb_buf[0x02]   = 0x01;
    usb_buf[0x03]   = 0x01;

    /*-----------------------------------------------------*\
    | Send packets                                          |
    \*-----------------------------------------------------*/
    hid_write(dev, usb_buf, 65);

    usb_buf[0x02]   = 0x00;

    hid_write(dev, usb_buf, 65);
}

unsigned short AuraMainboardController::GetMask(int start, int size)
{
    return(((1 << size) - 1) << start);
}

void AuraMainboardController::SendEffect
    (
    unsigned char   channel,
    unsigned char   mode,
    bool            shutdown_effect
    )
{
    unsigned char usb_buf[65];

    /*-----------------------------------------------------*\
    | Zero out buffer                                       |
    \*-----------------------------------------------------*/
    memset(usb_buf, 0x00, sizeof(usb_buf));

    /*-----------------------------------------------------*\
    | Set up message packet                                 |
    \*-----------------------------------------------------*/
    usb_buf[0x00]   = 0xEC;
    usb_buf[0x01]   = AURA_MAINBOARD_CONTROL_MODE_EFFECT;
    usb_buf[0x02]   = channel;
    usb_buf[0x03]   = 0x00;
    usb_buf[0x04]   = shutdown_effect ? 0x01 : 0x00;
    usb_buf[0x05]   = mode;

    /*-----------------------------------------------------*\
    | Send packet                                           |
    \*-----------------------------------------------------*/
    hid_write(dev, usb_buf, 65);
}

void AuraMainboardController::SendColor
    (
    unsigned char   /*channel*/,
    unsigned char   start_led,
    unsigned char   led_count,
    unsigned char*  led_data,
    bool            shutdown_effect
    )
{
    unsigned short  mask = GetMask(start_led, led_count);
    unsigned char   usb_buf[65];

    /*-----------------------------------------------------*\
    | Zero out buffer                                       |
    \*-----------------------------------------------------*/
    memset(usb_buf, 0x00, sizeof(usb_buf));

    /*-----------------------------------------------------*\
    | Set up message packet                                 |
    \*-----------------------------------------------------*/
    usb_buf[0x00]   = 0xEC;
    usb_buf[0x01]   = AURA_MAINBOARD_CONTROL_MODE_EFFECT_COLOR;
    usb_buf[0x02]   = mask >> 8;
    usb_buf[0x03]   = mask & 0xff;
    usb_buf[0x04]   = shutdown_effect ? 0x01 : 0x00;

    /*-----------------------------------------------------*\
    | Copy in color data bytes                              |
    \*-----------------------------------------------------*/
    memcpy(&usb_buf[0x05 + 3 * start_led], led_data, led_count * 3);

    /*-----------------------------------------------------*\
    | Send packet                                           |
    \*-----------------------------------------------------*/
    hid_write(dev, usb_buf, 65);
}

void AuraMainboardController::SendCommit()
{
    unsigned char usb_buf[65];

    /*-----------------------------------------------------*\
    | Zero out buffer                                       |
    \*-----------------------------------------------------*/
    memset(usb_buf, 0x00, sizeof(usb_buf));

    /*-----------------------------------------------------*\
    | Set up message packet                                 |
    \*-----------------------------------------------------*/
    usb_buf[0x00]   = 0xEC;
    usb_buf[0x01]   = AURA_MAINBOARD_CONTROL_MODE_COMMIT;
    usb_buf[0x02]   = 0x55;

    /*-----------------------------------------------------*\
    | Send packet                                           |
    \*-----------------------------------------------------*/
    hid_write(dev, usb_buf, 65);
}
