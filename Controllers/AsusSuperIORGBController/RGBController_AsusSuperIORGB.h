/*---------------------------------------------------------*\
| RGBController_AsusSuperIORGB.h                            |
|                                                           |
|   RGBController for the ASUS Super I/O 12V RGB header     |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include "RGBController.h"
#include "AsusSuperIORGBController.h"

class RGBController_AsusSuperIORGB : public RGBController
{
public:
    RGBController_AsusSuperIORGB(AsusSuperIORGBController* controller_ptr);
    ~RGBController_AsusSuperIORGB();

    void        SetupZones();

    void        DeviceUpdateLEDs();
    void        DeviceUpdateZoneLEDs(int zone);
    void        DeviceUpdateSingleLED(int led);

    void        DeviceUpdateMode();

private:
    AsusSuperIORGBController*   controller;
};
