/*---------------------------------------------------------*\
| RGBController_GloriousO3.cpp                              |
|                                                           |
|   RGBController for the Glorious Model O3 Wireless        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "RGBController_GloriousO3.h"

/**------------------------------------------------------------------*\
    @name Glorious Model O3 Wireless
    @category Mouse
    @type USB
    @save :x:
    @direct :x:
    @effects :x:
    @detectors DetectGloriousO3Controllers
    @comment Experimental, static color only, 2.4 GHz receiver 3794:A300
\*-------------------------------------------------------------------*/

RGBController_GloriousO3::RGBController_GloriousO3(GloriousO3Controller* controller_ptr)
{
    controller                          = controller_ptr;

    name                                = controller->GetDeviceName();
    vendor                              = "Glorious";
    type                                = DEVICE_TYPE_MOUSE;
    description                         = "Glorious Model O3 Wireless (Experimental)";
    location                            = controller->GetDeviceLocation();

    /*-----------------------------------------------------*\
    | Named "Static" so hosts that pick Direct, Custom or    |
    | Static for per-LED control select it                   |
    \*-----------------------------------------------------*/
    mode Static;
    Static.name                         = "Static";
    Static.value                        = 0x05;
    Static.flags                        = MODE_FLAG_HAS_PER_LED_COLOR;
    Static.color_mode                   = MODE_COLORS_PER_LED;
    modes.push_back(Static);

    mode Off;
    Off.name                            = "Off";
    Off.value                           = 0x00;
    Off.flags                           = 0;
    Off.color_mode                      = MODE_COLORS_NONE;
    modes.push_back(Off);

    SetupZones();
}

RGBController_GloriousO3::~RGBController_GloriousO3()
{
    Shutdown();

    delete controller;
}

void RGBController_GloriousO3::SetupZones()
{
    zone new_zone;
    new_zone.name       = "Mouse";
    new_zone.type       = ZONE_TYPE_SINGLE;
    new_zone.leds_min   = 1;
    new_zone.leds_max   = 1;
    new_zone.leds_count = 1;
    zones.push_back(new_zone);

    led new_led;
    new_led.name = "Mouse";
    leds.push_back(new_led);

    SetupColors();
}

void RGBController_GloriousO3::DeviceUpdateLEDs()
{
    DeviceUpdateMode();
}

void RGBController_GloriousO3::DeviceUpdateZoneLEDs(int /*zone*/)
{
    DeviceUpdateMode();
}

void RGBController_GloriousO3::DeviceUpdateSingleLED(int /*led*/)
{
    DeviceUpdateMode();
}

void RGBController_GloriousO3::DeviceUpdateMode()
{
    if(modes[active_mode].value == 0x00)
    {
        controller->SetOff();
    }
    else
    {
        controller->SetStaticColor(colors[0]);
    }
}
