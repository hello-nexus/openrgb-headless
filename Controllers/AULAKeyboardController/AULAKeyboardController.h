/*---------------------------------------------------------*\
| AULAKeyboardController.h                                  |
|                                                           |
|   Driver for AULA F75 Max keyboard (Sonix)                |
|                                                           |
|   Nexus                                       05 Jul 2026 |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <string>
#include <hidapi.h>
#include "RGBController.h"

#define AULA_KB_REPORT_SIZE     ( 0x41 )/* 65-byte HID output report    */
                                        /* (report id 0x00 + 64 bytes)  */
#define AULA_KB_DATA_SIZE       ( 0x40 )/* 64 data bytes per chunk      */

class AULAKeyboardController
{
public:
    AULAKeyboardController(hid_device* dev_handle, const char* path, std::string dev_name);
    ~AULAKeyboardController();

    std::string GetDeviceLocation();
    std::string GetNameString();
    std::string GetSerialString();

    void        SetKeyboardColors
                    (
                    unsigned char *     quad_data,
                    unsigned int        size
                    );

private:
    hid_device*             dev;
    std::string             location;
    std::string             name;

    void        SendKeyboardBegin();

    void        SendKeyboardData
                    (
                    unsigned char *     data
                    );

    void        SendKeyboardEnd();
};
