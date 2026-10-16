/*---------------------------------------------------------*\
| GloriousO3Controller.h                                    |
|                                                           |
|   Driver for the Glorious Model O3 Wireless (2.4 GHz      |
|   receiver), experimental static color only              |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <hidapi.h>
#include "RGBController.h"

class GloriousO3Controller
{
public:
    GloriousO3Controller(hid_device* dev_handle, const char* path, std::string dev_name);
    ~GloriousO3Controller();

    std::string     GetDeviceLocation();
    std::string     GetDeviceName();

    /*-----------------------------------------------------*\
    | Both calls only queue the value. A worker thread sends |
    | at most one report per cooldown and always sends the   |
    | most recently queued value, so the last color wins.    |
    \*-----------------------------------------------------*/
    void            SetStaticColor(RGBColor color);
    void            SetOff();

private:
    struct Value
    {
        bool            enabled;
        unsigned char   r;
        unsigned char   g;
        unsigned char   b;

        bool operator==(const Value& other) const
        {
            return(enabled == other.enabled && r == other.r && g == other.g && b == other.b);
        }
    };

    void            Queue(const Value& value);
    void            WorkerLoop();
    bool            SendReport(const Value& value);

    hid_device*                             dev;
    std::string                             location;
    std::string                             name;

    std::mutex                              mutex;
    std::condition_variable                 wake;
    bool                                    stop;
    bool                                    has_pending;
    Value                                   pending;
    bool                                    have_sent;
    Value                                   last_sent;
    std::chrono::steady_clock::time_point   last_write;
    std::thread                             worker;
};
