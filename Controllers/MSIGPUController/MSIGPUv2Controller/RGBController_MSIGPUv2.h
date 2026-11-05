/*---------------------------------------------------------*\
| RGBController_MSIGPUv2.h                                  |
|                                                           |
|   RGBController for MSI V2 GPU (ITE9)                     |
|                                                           |
|   Wojciech Lazarski                           03 Jan 2023 |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <atomic>
#include <chrono>

#include "pci_ids.h"
#include "RGBController.h"
#include "MSIGPUv2Controller.h"

class RGBController_MSIGPUv2 : public RGBController
{
public:
    RGBController_MSIGPUv2(MSIGPUv2Controller* controller_ptr, int msi_gpu_id);
    ~RGBController_MSIGPUv2();

    void        SetupZones();

    void        DeviceUpdateLEDs();
    void        DeviceUpdateZoneLEDs(int zone);
    void        DeviceUpdateSingleLED(int led);

    void        DeviceUpdateMode();
    void        DeviceSaveMode();

private:
    MSIGPUv2Controller* controller;
    bool                rainbow_variants;
    std::atomic<bool>   first_apply_logged{false};

    /*-----------------------------------------------------*\
    | State of the last full Direct sequence, so later      |
    | Direct frames can skip unchanged colors and the       |
    | brightness write, and be rate limited                 |
    \*-----------------------------------------------------*/
    bool                direct_primed       = false;
    unsigned char       direct_brightness   = 0;
    unsigned char       direct_rgb[3]       = { 0, 0, 0 };
    std::chrono::steady_clock::time_point direct_write_time;

    void       DeviceUpdateAll(const mode& current_mode, bool allow_color_only);
};
