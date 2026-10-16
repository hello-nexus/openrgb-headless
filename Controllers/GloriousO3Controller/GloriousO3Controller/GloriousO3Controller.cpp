/*---------------------------------------------------------*\
| GloriousO3Controller.cpp                                  |
|                                                           |
|   Driver for the Glorious Model O3 Wireless (2.4 GHz      |
|   receiver), experimental static color only              |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <cstring>
#include "GloriousO3Controller.h"
#include "LogManager.h"

/*---------------------------------------------------------*\
| Whether these reports write the mouse's flash is unknown, |
| so the write rate is capped. Latest value always wins.    |
\*---------------------------------------------------------*/
static const std::chrono::milliseconds GLORIOUS_O3_WRITE_COOLDOWN(1000);

GloriousO3Controller::GloriousO3Controller(hid_device* dev_handle, const char* path, std::string dev_name)
{
    dev         = dev_handle;
    location    = path;
    name        = dev_name;

    stop        = false;
    has_pending = false;
    pending     = {false, 0, 0, 0};
    have_sent   = false;
    last_sent   = {false, 0, 0, 0};
    last_write  = std::chrono::steady_clock::now() - GLORIOUS_O3_WRITE_COOLDOWN;

    worker      = std::thread(&GloriousO3Controller::WorkerLoop, this);
}

GloriousO3Controller::~GloriousO3Controller()
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        stop = true;
    }
    wake.notify_all();
    worker.join();

    hid_close(dev);
}

std::string GloriousO3Controller::GetDeviceLocation()
{
    return("HID: " + location);
}

std::string GloriousO3Controller::GetDeviceName()
{
    return(name);
}

void GloriousO3Controller::SetStaticColor(RGBColor color)
{
    Queue({true, (unsigned char)RGBGetRValue(color), (unsigned char)RGBGetGValue(color), (unsigned char)RGBGetBValue(color)});
}

void GloriousO3Controller::SetOff()
{
    Queue({false, 0, 0, 0});
}

void GloriousO3Controller::Queue(const Value& value)
{
    {
        std::lock_guard<std::mutex> lock(mutex);

        /*-------------------------------------------------*\
        | Identical to what the device already shows and    |
        | nothing newer is waiting: nothing to send         |
        \*-------------------------------------------------*/
        if(!has_pending && have_sent && value == last_sent)
        {
            return;
        }

        pending     = value;
        has_pending = true;
    }
    wake.notify_all();
}

void GloriousO3Controller::WorkerLoop()
{
    std::unique_lock<std::mutex> lock(mutex);

    while(!stop)
    {
        wake.wait(lock, [this] { return(stop || has_pending); });

        if(stop)
        {
            break;
        }

        /*-------------------------------------------------*\
        | Wait out the cooldown; newer values replace the   |
        | pending one meanwhile                             |
        \*-------------------------------------------------*/
        wake.wait_until(lock, last_write + GLORIOUS_O3_WRITE_COOLDOWN, [this] { return(stop); });

        if(stop)
        {
            break;
        }

        Value value = pending;
        has_pending = false;

        if(have_sent && value == last_sent)
        {
            continue;
        }

        lock.unlock();
        bool ok = SendReport(value);
        lock.lock();

        last_write = std::chrono::steady_clock::now();

        if(ok)
        {
            have_sent = true;
            last_sent = value;
        }
    }
}

bool GloriousO3Controller::SendReport(const Value& value)
{
    /*-----------------------------------------------------*\
    | Feature report 0x03, 64 bytes, vendor collection      |
    | (usage page 0xFF00, usage 0x0001) of the receiver.    |
    | Layout taken from a contributor's captured packets.   |
    \*-----------------------------------------------------*/
    unsigned char report[64];
    memset(report, 0x00, sizeof(report));

    report[0]   = 0x03;
    report[1]   = 0xBB;
    report[2]   = 0x14;
    report[3]   = 0x0A;
    report[4]   = value.enabled ? 0x05 : 0x00;
    report[5]   = 0x64;
    report[6]   = 0x64;
    report[7]   = 0x00;
    report[8]   = 0x32;
    report[9]   = 0x08;
    report[10]  = value.enabled ? 0x01 : 0x00;
    report[11]  = value.enabled ? value.r : 0x00;
    report[12]  = value.enabled ? value.g : 0x00;
    report[13]  = value.enabled ? value.b : 0x00;

    int sent = hid_send_feature_report(dev, report, sizeof(report));

    if(sent != (int)sizeof(report))
    {
        LOG_ERROR("[%s] Failure to send report (%ls)!", name.c_str(), hid_error(dev));
        return(false);
    }

    return(true);
}
