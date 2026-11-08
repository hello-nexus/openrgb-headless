/*---------------------------------------------------------*\
| AsusSuperIORGBControllerDetect.cpp                        |
|                                                           |
|   Detector for the ASUS Super I/O 12V RGB header          |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "AsusSuperIORGBController.h"
#include "DetectionManager.h"
#include "dmiinfo.h"
#include "LogManager.h"
#include "RGBController_AsusSuperIORGB.h"
#include "super_io.h"

#define NUM_COMPATIBLE_DEVICES (sizeof(compatible_devices) / sizeof(compatible_devices[0]))

typedef struct
{
    const char*     name;
    int             sioaddr;
    unsigned int    chip_id;
} asus_sio_device;

/*---------------------------------------------------------*\
| Exact baseboard product names: the register map is only   |
| known for these boards                                    |
\*---------------------------------------------------------*/
static asus_sio_device compatible_devices[] =
{
    { "PRIME H510M-E",  0x2E,   SIO_NCT6798_ID  },
};

DetectedControllers DetectAsusSuperIORGBControllers()
{
    DetectedControllers detected_controllers;
    DMIInfo             board;
    std::string         board_dmi       = board.getMainboard();
    std::string         manufacturer    = board.getManufacturer();

    if(manufacturer.find("ASUS") == std::string::npos)
    {
        return(detected_controllers);
    }

    for(unsigned int i = 0; i < NUM_COMPATIBLE_DEVICES; i++)
    {
        if(board_dmi != compatible_devices[i].name)
        {
            continue;
        }

        int sioaddr = compatible_devices[i].sioaddr;
        int chip_id = AsusSuperIORGBController::ReadChipID(sioaddr);

        if(chip_id < 0)
        {
            LOG_WARNING("[ASUS Super IO] %s: could not read the Super IO chip ID at 0x%02X", board_dmi.c_str(), sioaddr);
            break;
        }

        LOG_TRACE("[ASUS Super IO] %s: Super IO address 0x%02X probed value 0x%04X", board_dmi.c_str(), sioaddr, chip_id);

        if((unsigned int)(chip_id & SIO_ID_MASK) != compatible_devices[i].chip_id)
        {
            LOG_WARNING("[ASUS Super IO] %s: unexpected Super IO chip ID 0x%04X at 0x%02X", board_dmi.c_str(), chip_id, sioaddr);
            break;
        }

        AsusSuperIORGBController*     controller     = new AsusSuperIORGBController(sioaddr, "ASUS " + board_dmi);
        RGBController_AsusSuperIORGB* rgb_controller = new RGBController_AsusSuperIORGB(controller);

        detected_controllers.push_back(rgb_controller);
        break;
    }

    return(detected_controllers);
}

REGISTER_DETECTOR("ASUS Super IO RGB", DetectAsusSuperIORGBControllers);
