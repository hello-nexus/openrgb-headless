/*---------------------------------------------------------*\
| RGBController_AULAKeyboard.cpp                            |
|                                                           |
|   RGBController for AULA F75 Max keyboard                 |
|                                                           |
|   Nexus                                       05 Jul 2026 |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <cstring>
#include "RGBController_AULAKeyboard.h"
#include "RGBControllerKeyNames.h"

//0xFFFFFFFF indicates an unused entry in matrix
#define NA  0xFFFFFFFF

#define AULA_KB_LED_COUNT   80  /* 76 high-confidence + 4 unverified nav keys */

/*-----------------------------------------------------------------------------*\
| The firmware latches a frame only when it receives the full matrix, so every   |
| frame streams all 94 F98Pro matrix slots (padded to 7 x 64-byte chunks = 448   |
| bytes, matching the reference Aula F98Pro.js send exactly).  Mapped keys carry  |
| their colour; unpopulated matrix slots (the F98Pro numpad) go black.           |
\*-----------------------------------------------------------------------------*/
#define AULA_KB_FRAME_SLOTS 94
#define AULA_KB_FRAME_BYTES 448 /* 7 * 64 */

/*---------------------------------------------------------------------------*\
| TODO(phase2): No device is available to verify against, so this layout is    |
|               built blind.  The 4 right-side nav keys (Delete / Page Up /     |
|               Page Down / End) use UNVERIFIED best-effort hardware indices    |
|               (see the hw_index note below); they are guesses, not a pending  |
|               on-device task.  The rotary knob is intentionally given NO LED  |
|               and NO matrix cell: it is a bare metal encoder with a separate  |
|               TFT screen and almost certainly has no addressable per-key LED. |
\*---------------------------------------------------------------------------*/

static unsigned int matrix_map[6][15] =
    { {   0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  NA,  NA },
      {  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  76 },
      {  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  77 },
      {  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,  52,  53,  NA,  78 },
      {  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,  64,  65,  NA,  66,  79 },
      {  67,  68,  69,  NA,  NA,  70,  NA,  NA,  NA,  71,  72,  NA,  73,  74,  75 } };

/*-----------------------------------------------------------------*\
| Per-LED hardware index, parallel to the LED / led_names order.    |
\*-----------------------------------------------------------------*/
static const unsigned char hw_index[AULA_KB_LED_COUNT] =
{
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D,
    0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x67,
    0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x43,
    0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x55,
    0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x65,
    0x5B, 0x5C, 0x5D, 0x5E, 0x60, 0x62, 0x63, 0x64, 0x66,
    // UNVERIFIED blind guess (no device); may light the wrong key or nothing. Alt candidates: Delete 0x21/0x23, PgUp 0x33/0x34, PgDn 0x45/0x46, End 0x57/0x58.
    0x20, 0x32, 0x44, 0x56,   // Delete, Page Up, Page Down, End (nav column, physical rows 1-4)
};

/*-----------------------------------------------------------------*\
| Per-LED name, parallel to the LED / hw_index order.               |
\*-----------------------------------------------------------------*/
static const char* led_names[AULA_KB_LED_COUNT] =
{
    KEY_EN_ESCAPE,      KEY_EN_F1,          KEY_EN_F2,          KEY_EN_F3,
    KEY_EN_F4,          KEY_EN_F5,          KEY_EN_F6,          KEY_EN_F7,
    KEY_EN_F8,          KEY_EN_F9,          KEY_EN_F10,         KEY_EN_F11,
    KEY_EN_F12,
    KEY_EN_BACK_TICK,   KEY_EN_1,           KEY_EN_2,           KEY_EN_3,
    KEY_EN_4,           KEY_EN_5,           KEY_EN_6,           KEY_EN_7,
    KEY_EN_8,           KEY_EN_9,           KEY_EN_0,           KEY_EN_MINUS,
    KEY_EN_EQUALS,      KEY_EN_BACKSPACE,
    KEY_EN_TAB,         KEY_EN_Q,           KEY_EN_W,           KEY_EN_E,
    KEY_EN_R,           KEY_EN_T,           KEY_EN_Y,           KEY_EN_U,
    KEY_EN_I,           KEY_EN_O,           KEY_EN_P,           KEY_EN_LEFT_BRACKET,
    KEY_EN_RIGHT_BRACKET, KEY_EN_BACK_SLASH,
    KEY_EN_CAPS_LOCK,   KEY_EN_A,           KEY_EN_S,           KEY_EN_D,
    KEY_EN_F,           KEY_EN_G,           KEY_EN_H,           KEY_EN_J,
    KEY_EN_K,           KEY_EN_L,           KEY_EN_SEMICOLON,   KEY_EN_QUOTE,
    KEY_EN_ANSI_ENTER,
    KEY_EN_LEFT_SHIFT,  KEY_EN_Z,           KEY_EN_X,           KEY_EN_C,
    KEY_EN_V,           KEY_EN_B,           KEY_EN_N,           KEY_EN_M,
    KEY_EN_COMMA,       KEY_EN_PERIOD,      KEY_EN_FORWARD_SLASH, KEY_EN_RIGHT_SHIFT,
    KEY_EN_UP_ARROW,
    KEY_EN_LEFT_CONTROL, KEY_EN_LEFT_WINDOWS, KEY_EN_LEFT_ALT,  KEY_EN_SPACE,
    KEY_EN_LEFT_FUNCTION, KEY_EN_RIGHT_CONTROL, KEY_EN_LEFT_ARROW, KEY_EN_DOWN_ARROW,
    KEY_EN_RIGHT_ARROW,
    // UNVERIFIED nav column (see hw_index note above)
    KEY_EN_DELETE,      KEY_EN_PAGE_UP,     KEY_EN_PAGE_DOWN,   KEY_EN_END,
};

/*-----------------------------------------------------------------*\
| Full matrix send order (the F98Pro `indexes` sequence): every      |
| hardware slot the firmware expects per frame.  DeviceUpdateLEDs     |
| colours the slots present in hw_index and blacks the rest.  0x20    |
| (Delete guess) is absent from the F98Pro matrix, so Delete is not   |
| driven by this frame.                                               |
\*-----------------------------------------------------------------*/
static const unsigned char full_frame_indexes[AULA_KB_FRAME_SLOTS] =
{
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x13, 0x14, 0x15,
    0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x67, 0x77, 0x21, 0x22, 0x7A, 0x25,
    0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x43, 0x32, 0x33, 0x34,
    0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x55, 0x44, 0x45, 0x46,
    0x7B, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x65, 0x56, 0x57,
    0x58, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x62, 0x63, 0x64, 0x66, 0x68, 0x69, 0x6A,
};

/**------------------------------------------------------------------*\
    @name AULA F75 Max
    @category Keyboard
    @type USB
    @save :x:
    @direct :white_check_mark:
    @effects :x:
    @detectors DetectAULAKeyboards
    @comment
\*-------------------------------------------------------------------*/

RGBController_AULAKeyboard::RGBController_AULAKeyboard(AULAKeyboardController* controller_ptr)
{
    controller                      = controller_ptr;

    name                            = controller->GetNameString();
    vendor                          = "AULA";
    type                            = DEVICE_TYPE_KEYBOARD;
    description                     = "AULA F75 Max Keyboard Device";
    location                        = controller->GetDeviceLocation();
    serial                          = controller->GetSerialString();

    mode Direct;
    Direct.name                     = "Direct";
    Direct.value                    = 0;
    Direct.flags                    = MODE_FLAG_HAS_PER_LED_COLOR;
    Direct.color_mode               = MODE_COLORS_PER_LED;
    modes.push_back(Direct);

    SetupZones();
}

RGBController_AULAKeyboard::~RGBController_AULAKeyboard()
{
    delete controller;
}

void RGBController_AULAKeyboard::SetupZones()
{
    zone new_zone;

    new_zone.name               = "Keyboard";
    new_zone.type               = ZONE_TYPE_MATRIX;
    new_zone.leds_min           = AULA_KB_LED_COUNT;
    new_zone.leds_max           = AULA_KB_LED_COUNT;
    new_zone.leds_count         = AULA_KB_LED_COUNT;
    new_zone.matrix_map.Set(6, 15, (unsigned int *)&matrix_map);

    zones.push_back(new_zone);

    for(int led_idx = 0; led_idx < AULA_KB_LED_COUNT; led_idx++)
    {
        led new_led;

        new_led.name = led_names[led_idx];

        leds.push_back(new_led);
    }

    SetupColors();
}

void RGBController_AULAKeyboard::ResizeZone(int /*zone*/, int /*new_size*/)
{
    /*---------------------------------------------------------*\
    | This device does not support resizing zones               |
    \*---------------------------------------------------------*/
}

void RGBController_AULAKeyboard::DeviceUpdateLEDs()
{
    unsigned char quad_data[AULA_KB_FRAME_BYTES];

    /*-----------------------------------------------------*\
    | Zero-pad to the full 7-chunk frame; slots the board    |
    | does not populate stay black.                          |
    \*-----------------------------------------------------*/
    memset(quad_data, 0x00, sizeof(quad_data));

    for(unsigned int slot = 0; slot < AULA_KB_FRAME_SLOTS; slot++)
    {
        unsigned char   idx   = full_frame_indexes[slot];
        RGBColor        color = 0x00000000;

        for(int led_idx = 0; led_idx < AULA_KB_LED_COUNT; led_idx++)
        {
            if(hw_index[led_idx] == idx)
            {
                color = colors[led_idx];
                break;
            }
        }

        quad_data[(4 * slot) + 0] = idx;
        quad_data[(4 * slot) + 1] = RGBGetRValue(color);
        quad_data[(4 * slot) + 2] = RGBGetGValue(color);
        quad_data[(4 * slot) + 3] = RGBGetBValue(color);
    }

    controller->SetKeyboardColors(quad_data, AULA_KB_FRAME_BYTES);
}

void RGBController_AULAKeyboard::UpdateZoneLEDs(int /*zone*/)
{
    DeviceUpdateLEDs();
}

void RGBController_AULAKeyboard::UpdateSingleLED(int /*led*/)
{
    DeviceUpdateLEDs();
}

void RGBController_AULAKeyboard::DeviceUpdateMode()
{
    /*---------------------------------------------------------*\
    | Direct mode only - nothing to configure on the device     |
    \*---------------------------------------------------------*/
}
