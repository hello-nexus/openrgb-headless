/*---------------------------------------------------------*\
| AULAKeyboardControllerDetect.cpp                          |
|                                                           |
|   Detector for AULA F75 Max keyboard                      |
|                                                           |
|   Nexus                                       05 Jul 2026 |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <hidapi.h>
#include "DetectionManager.h"
#include "AULAKeyboardController.h"
#include "RGBController_AULAKeyboard.h"

/*-----------------------------------------------------*\
| Keyboard product IDs                                  |
\*-----------------------------------------------------*/
#define AULA_KEYBOARD_VID           0x0C45
#define AULA_F75_MAX_PID            0x800A
#define AULA_KEYBOARD_USAGE_PAGE    0xFF13
#define AULA_KEYBOARD_USAGE         0x0001

/******************************************************************************************\
*                                                                                          *
*   DetectAULAKeyboards                                                                    *
*                                                                                          *
*       Tests the USB address to see if an AULA RGB Keyboard controller exists there.      *
*                                                                                          *
\******************************************************************************************/

DetectedControllers DetectAULAKeyboards(hid_device_info* info, const std::string& name)
{
    DetectedControllers detected_controllers;

    hid_device* dev = hid_open_path(info->path);

    if(dev)
    {
        AULAKeyboardController*     controller     = new AULAKeyboardController(dev, info->path, name);
        RGBController_AULAKeyboard* rgb_controller = new RGBController_AULAKeyboard(controller);

        detected_controllers.push_back(rgb_controller);
    }

    return(detected_controllers);
}

/*---------------------------------------------------------------------------------------------------------------------------------------------*\
| Keyboards                                                                                                                                     |
\*---------------------------------------------------------------------------------------------------------------------------------------------*/
REGISTER_HID_DETECTOR_PU("AULA F75 Max", DetectAULAKeyboards, AULA_KEYBOARD_VID, AULA_F75_MAX_PID, AULA_KEYBOARD_USAGE_PAGE, AULA_KEYBOARD_USAGE);
