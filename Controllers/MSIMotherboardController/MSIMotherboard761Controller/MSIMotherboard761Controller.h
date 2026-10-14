/*---------------------------------------------------------*\
| MSIMotherboard761Controller.h                             |
|                                                           |
|   Driver for MSI Mystic Light 761-byte motherboard        |
|                                                           |
|   Direct mode functionality has been implemented based on |
|   the SignalRGB project                                   |
|   (https://signalrgb.com/)                                |
|                                                           |
|   rom4ster                                    11 Jun 2025 |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <chrono>
#include <cstring>
#include <hidapi.h>
#include "MSIMotherboardCommon.h"
#include "RGBController.h"

inline constexpr const char * BOARD_UNSUPPORTED_ERROR =  "No Config Found For Board";

/*---------------------------------------------------------*\
| Min/avg/max of HID write durations over one log window    |
\*---------------------------------------------------------*/
struct MSI761WriteStat
{
    unsigned int    count   = 0;
    double          min_ms  = 0.0;
    double          max_ms  = 0.0;
    double          sum_ms  = 0.0;

    void Add(double ms)
    {
        if((count == 0) || (ms < min_ms))
        {
            min_ms = ms;
        }
        if(ms > max_ms)
        {
            max_ms = ms;
        }
        sum_ms += ms;
        count++;
    }

    double Avg() const
    {
        return((count > 0) ? (sum_ms / count) : 0.0);
    }
};

class MSIMotherboard761Controller
{
public:
    MSIMotherboard761Controller
                    (
                    hid_device*     handle,
                    const char*     path,
                    std::string     dev_name
                    );

    ~MSIMotherboard761Controller();

    void        SetMode
                    (
                    MSI_ZONE        zone,
                    MSI_MODE        mode,
                    MSI_SPEED       speed,
                    MSI_BRIGHTNESS  brightness,
                    bool            rainbow_color
                    );

    MSI_MODE    GetMode();

    void        GetMode
                    (
                    MSI_ZONE        zone,
                    MSI_MODE        &mode,
                    MSI_SPEED       &speed,
                    MSI_BRIGHTNESS  &brightness,
                    bool            &rainbow_color,
                    unsigned int    &color
                    );

    void        SetZoneColor
                    (
                    MSI_ZONE        zone,
                    unsigned char   red1,
                    unsigned char   grn1,
                    unsigned char   blu1,
                    unsigned char   red2,
                    unsigned char   grn2,
                    unsigned char   blu2
                    );

    void        SetLedColor
                    (
                    MSI_ZONE        zone,
                    std::size_t     index,
                    unsigned char   red,
                    unsigned char   grn,
                    unsigned char   blu
                    );

    void        SetZoneLEDCount
                    (
                    MSI_ZONE        zone,
                    unsigned char   led_num
                    );

    void        SendConfiguration();

    bool        Update
                    (
                    bool save
                    );

    std::string     GetDeviceName();
    std::string     GetDeviceLocation();
    std::string     GetFWVersion();
    std::string     GetSerial();

    void        SetDirectMode
                    (
                    bool mode
                    );

    bool        IsDirectModeActive();

    size_t      GetMaxDirectLeds
                    (
                    MSI_ZONE zone
                    );

    const std::vector<MSI_ZONE>*
    GetSupportedZones()
    {
        return supported_zones;
    }

    enum DIRECT_MODE
    {
        DIRECT_MODE_DISABLED,
        DIRECT_MODE_PER_LED,
        DIRECT_MODE_ZONE_BASED
    };

    DIRECT_MODE     GetSupportedDirectMode()
    {
        return DIRECT_MODE_PER_LED;
    }

    struct ZoneConfig
    {
        MSI_ZONE msi_zone;
        ZoneData * zone_data;
    };

private:
    std::string                                             name;
    std::vector<MSIMotherboard761Controller::ZoneConfig>    zone_configs;
    const std::vector<MSI_ZONE>*                            supported_zones;
    hid_device*                                             dev;
    std::string                                             location;
    std::string                                             version_aprom;
    std::string                                             version_ldrom;
    FeaturePacket_761* data;
    unsigned char*                                          setup_data;
    bool                                                    setup_dirty;
    int                                                     numof_pipe1_leds;

    /*-----------------------------------------------------*\
    | Write timing diagnostics, summarized every 5 seconds  |
    \*-----------------------------------------------------*/
    MSI761WriteStat                                         timing_report;
    MSI761WriteStat                                         timing_update;
    MSI761WriteStat                                         timing_setup;
    unsigned int                                            timing_failures;
    int                                                     timing_last_error;
    std::chrono::steady_clock::time_point                   timing_window_start;

    void            LogWriteTiming();

    bool            ReadSettings();
    bool            ReadFwVersion();
    void            ReadName();

    ZoneData* GetZoneData
            (
            FeaturePacket_761&  data_packet,
            MSI_ZONE            zone
            );
    RainbowZoneData* GetRainbowZoneData(MSI_ZONE zone);
    Color* GetPerLedZoneData
            (
            MSI_ZONE zone
            );
    void            SelectPerLedProtocol();
};
