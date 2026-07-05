/*---------------------------------------------------------*\
| RGBController_AULAKeyboard.h                              |
|                                                           |
|   RGBController for AULA F75 Max keyboard                 |
|                                                           |
|   Nexus                                       05 Jul 2026 |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include "RGBController.h"
#include "AULAKeyboardController.h"

class RGBController_AULAKeyboard : public RGBController
{
public:
    RGBController_AULAKeyboard(AULAKeyboardController* controller_ptr);
    ~RGBController_AULAKeyboard();

    void        SetupZones();

    void        ResizeZone(int zone, int new_size);

    void        DeviceUpdateLEDs();
    void        UpdateZoneLEDs(int zone);
    void        UpdateSingleLED(int led);

    void        DeviceUpdateMode();

private:
    AULAKeyboardController*  controller;
};
