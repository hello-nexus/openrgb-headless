/*---------------------------------------------------------*\
| AsusSuperIORGBController.h                                |
|                                                           |
|   Driver for the 12V RGB header that ASUS boards without  |
|   an Aura chip drive through the Nuvoton Super I/O        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <string>

/*---------------------------------------------------------*\
| Logical devices and registers observed on a PRIME         |
| H510M-E (NCT6798D). Only these are ever written.          |
\*---------------------------------------------------------*/
#define ASUS_SIO_LOGDEV_PREFIX      0x12    /* Read and written back unchanged first */
#define ASUS_SIO_LOGDEV_LED         0x13    /* LED engine: mode, stage colors       */
#define ASUS_SIO_LOGDEV_PARAMS      0x15    /* Per-mode parameter block             */

#define ASUS_SIO_REG_PREFIX         0xED
#define ASUS_SIO_REG_MODE           0xFF
#define ASUS_SIO_REG_STAGES         0xFC

#define ASUS_SIO_EXIT_KEY           0xAA

enum
{
    ASUS_SIO_MODE_STATIC        = 0x01,
    ASUS_SIO_MODE_OFF           = 0x03,
    ASUS_SIO_MODE_BLINKING      = 0x05,
    ASUS_SIO_MODE_CYCLE         = 0x07,     /* Strobing and Fading, told apart by LDN 0x15 */
};

enum
{
    ASUS_SIO_EFFECT_STATIC,
    ASUS_SIO_EFFECT_OFF,
    ASUS_SIO_EFFECT_BLINKING,
    ASUS_SIO_EFFECT_STROBING,
    ASUS_SIO_EFFECT_FADING,
};

enum
{
    ASUS_SIO_SPEED_SLOW         = 0,
    ASUS_SIO_SPEED_MIDDLE       = 1,
    ASUS_SIO_SPEED_FAST         = 2,
};

class AsusSuperIORGBController
{
public:
    AsusSuperIORGBController(int sioaddr, std::string dev_name);
    ~AsusSuperIORGBController();

    static int      ReadChipID(int sioaddr);

    std::string     GetDeviceName();
    std::string     GetDeviceLocation();

    void            Apply(unsigned int effect, unsigned int speed, unsigned char red, unsigned char green, unsigned char blue);

private:
    int             sioaddr;
    std::string     name;

    bool            last_valid;
    unsigned int    last_effect;
    unsigned int    last_speed;
    unsigned char   last_red;
    unsigned char   last_green;
    unsigned char   last_blue;

    bool            Write(unsigned int effect, unsigned int speed, unsigned char red, unsigned char green, unsigned char blue);
};
