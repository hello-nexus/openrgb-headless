/*---------------------------------------------------------*\
| RGBController_GloriousO3.h                                |
|                                                           |
|   RGBController for the Glorious Model O3 Wireless        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include "RGBController.h"
#include "GloriousO3Controller.h"

class RGBController_GloriousO3 : public RGBController
{
public:
    RGBController_GloriousO3(GloriousO3Controller* controller_ptr);
    ~RGBController_GloriousO3();

    void        SetupZones();

    void        DeviceUpdateLEDs();
    void        DeviceUpdateZoneLEDs(int zone);
    void        DeviceUpdateSingleLED(int led);

    void        DeviceUpdateMode();

private:
    GloriousO3Controller*   controller;
};
