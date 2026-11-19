/*---------------------------------------------------------*\
| RGBController_SkyloongGK68HEPro.h                         |
|                                                           |
|   RGBController for Skyloong GK68HE Pro keyboard          |
|                                                           |
|   Titan                                       05 Aug 2026 |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include "RGBController.h"
#include "RoyuanKeyboardController.h"

#define GK68HE_PRO_LED_COUNT                67

struct GK68HEProLayout
{
    const char*                 name;
    const char*                 vendor;
    const char*                 description;
    unsigned int                led_count;
    unsigned int                matrix_rows;
    unsigned int                matrix_cols;
    const unsigned int*         matrix;
    const unsigned char*        slots;
    const char* const*          led_names;
};

extern const GK68HEProLayout gk68he_pro_layout;
extern const GK68HEProLayout vgn_neon75he_layout;

class RGBController_SkyloongGK68HEPro : public RGBController
{
public:
    RGBController_SkyloongGK68HEPro(RoyuanKeyboardController* controller_ptr, const GK68HEProLayout* layout_ptr = &gk68he_pro_layout);
    ~RGBController_SkyloongGK68HEPro();

    void        SetupZones();

    void        DeviceUpdateLEDs();
    void        UpdateZoneLEDs(int zone);
    void        UpdateSingleLED(int led);

    void        DeviceUpdateMode();

private:
    RoyuanKeyboardController* controller;
    const GK68HEProLayout*    layout;

    void SendCustomLayer();
    bool LayerColorsChanged();

    unsigned char flashed_colors[ROYUAN_SKYLOONG_GK68HE_PRO_USERPIC_SLOTS * 3];
    bool          layer_flashed = false;

    std::vector<unsigned char> mode_options;
};
