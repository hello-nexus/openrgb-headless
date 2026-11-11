/*---------------------------------------------------------*\
| RGBController_AsusSuperIORGB.cpp                          |
|                                                           |
|   RGBController for the ASUS Super I/O 12V RGB header     |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "RGBController_AsusSuperIORGB.h"

/**------------------------------------------------------------------*\
    @name ASUS Super IO RGB
    @category Motherboard
    @type SuperIO
    @save :x:
    @direct :white_check_mark:
    @effects :white_check_mark:
    @detectors DetectAsusSuperIORGBControllers
    @comment
        The single 12V RGB header of ASUS boards that have no Aura
        controller and drive the header through the Nuvoton Super IO
        (logical devices 0x13 and 0x15). 4 bits per color channel.

        Supported boards:
        | Board          | Super IO        |
        | :---:          | :---:           |
        | PRIME H510M-E  | NCT6798D (D42B) |
\*-------------------------------------------------------------------*/

RGBController_AsusSuperIORGB::RGBController_AsusSuperIORGB(AsusSuperIORGBController* controller_ptr)
{
    controller              = controller_ptr;

    name                    = controller->GetDeviceName();
    vendor                  = "ASUS";
    type                    = DEVICE_TYPE_MOTHERBOARD;
    description             = "ASUS Super IO RGB Device";
    location                = controller->GetDeviceLocation();

    mode Direct;
    Direct.name             = "Direct";
    Direct.value            = ASUS_SIO_EFFECT_STATIC;
    Direct.flags            = MODE_FLAG_HAS_PER_LED_COLOR;
    Direct.color_mode       = MODE_COLORS_PER_LED;
    modes.push_back(Direct);

    mode Static;
    Static.name             = "Static";
    Static.value            = ASUS_SIO_EFFECT_STATIC;
    Static.flags            = MODE_FLAG_HAS_PER_LED_COLOR;
    Static.color_mode       = MODE_COLORS_PER_LED;
    modes.push_back(Static);

    mode Off;
    Off.name                = "Off";
    Off.value               = ASUS_SIO_EFFECT_OFF;
    Off.flags               = 0;
    Off.color_mode          = MODE_COLORS_NONE;
    modes.push_back(Off);

    mode Blinking;
    Blinking.name           = "Blinking";
    Blinking.value          = ASUS_SIO_EFFECT_BLINKING;
    Blinking.flags          = MODE_FLAG_HAS_PER_LED_COLOR | MODE_FLAG_HAS_SPEED;
    Blinking.speed_min      = ASUS_SIO_SPEED_SLOW;
    Blinking.speed_max      = ASUS_SIO_SPEED_FAST;
    Blinking.speed          = ASUS_SIO_SPEED_SLOW;
    Blinking.color_mode     = MODE_COLORS_PER_LED;
    modes.push_back(Blinking);

    mode Strobing;
    Strobing.name           = "Strobing";
    Strobing.value          = ASUS_SIO_EFFECT_STROBING;
    Strobing.flags          = MODE_FLAG_HAS_PER_LED_COLOR | MODE_FLAG_HAS_SPEED;
    Strobing.speed_min      = ASUS_SIO_SPEED_SLOW;
    Strobing.speed_max      = ASUS_SIO_SPEED_FAST;
    Strobing.speed          = ASUS_SIO_SPEED_SLOW;
    Strobing.color_mode     = MODE_COLORS_PER_LED;
    modes.push_back(Strobing);

    mode Fading;
    Fading.name             = "Fading";
    Fading.value            = ASUS_SIO_EFFECT_FADING;
    Fading.flags            = MODE_FLAG_HAS_PER_LED_COLOR;
    Fading.color_mode       = MODE_COLORS_PER_LED;
    modes.push_back(Fading);

    SetupZones();
}

RGBController_AsusSuperIORGB::~RGBController_AsusSuperIORGB()
{
    Shutdown();

    delete controller;
}

void RGBController_AsusSuperIORGB::SetupZones()
{
    zone asus_zone;
    asus_zone.name          = "12V RGB Header";
    asus_zone.type          = ZONE_TYPE_SINGLE;
    asus_zone.leds_min      = 1;
    asus_zone.leds_max      = 1;
    asus_zone.leds_count    = 1;
    zones.push_back(asus_zone);

    led asus_led;
    asus_led.name           = "12V RGB Header";
    leds.push_back(asus_led);

    SetupColors();
}

void RGBController_AsusSuperIORGB::DeviceUpdateLEDs()
{
    const mode&   active    = modes[active_mode];
    RGBColor      color     = colors[0];
    unsigned int  speed     = (active.flags & MODE_FLAG_HAS_SPEED) ? active.speed : ASUS_SIO_SPEED_SLOW;

    controller->Apply(active.value, speed, RGBGetRValue(color), RGBGetGValue(color), RGBGetBValue(color));
}

void RGBController_AsusSuperIORGB::DeviceUpdateZoneLEDs(int /*zone*/)
{
    DeviceUpdateLEDs();
}

void RGBController_AsusSuperIORGB::DeviceUpdateSingleLED(int /*led*/)
{
    DeviceUpdateLEDs();
}

void RGBController_AsusSuperIORGB::DeviceUpdateMode()
{
    DeviceUpdateLEDs();
}
