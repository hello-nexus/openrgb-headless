/*---------------------------------------------------------*\
| GloriousO3ControllerDetect.cpp                            |
|                                                           |
|   Detector for the Glorious Model O3 Wireless (2.4 GHz    |
|   receiver)                                               |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <hidapi.h>
#include <string>
#include "DetectionManager.h"
#include "GloriousO3Controller.h"
#include "RGBController_GloriousO3.h"

#define GLORIOUS_O3_VID                     0x3794
#define GLORIOUS_O3_WIRELESS_RECEIVER_PID   0xA300

/*---------------------------------------------------------*\
| Detection only opens the vendor collection; it sends      |
| nothing to the device                                     |
\*---------------------------------------------------------*/
DetectedControllers DetectGloriousO3Controllers(hid_device_info* info, const std::string& name)
{
    DetectedControllers detected_controllers;
    hid_device*         dev;

    dev = hid_open_path(info->path);

    if(dev)
    {
        GloriousO3Controller*       controller      = new GloriousO3Controller(dev, info->path, name);
        RGBController_GloriousO3*   rgb_controller  = new RGBController_GloriousO3(controller);

        detected_controllers.push_back(rgb_controller);
    }

    return(detected_controllers);
}

REGISTER_HID_DETECTOR_IPU("Glorious Model O3 Wireless (Experimental)", DetectGloriousO3Controllers, GLORIOUS_O3_VID, GLORIOUS_O3_WIRELESS_RECEIVER_PID, 0, 0xFF00, 0x01);
