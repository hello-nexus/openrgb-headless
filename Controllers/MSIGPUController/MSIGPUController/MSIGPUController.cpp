/*---------------------------------------------------------*\
| MSIGPUController.cpp                                      |
|                                                           |
|   Driver for MSI GPU                                      |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "MSIGPUController.h"
#include "LogManager.h"
#include <cstring>

MSIGPUController::MSIGPUController(i2c_smbus_interface* bus, msi_gpu_dev_id dev, std::string dev_name)
{
    this->bus   = bus;
    this->dev   = dev;
    this->name  = dev_name;

    retry_recovery_logged = false;
}

MSIGPUController::~MSIGPUController()
{

}

std::string MSIGPUController::GetDeviceLocation()
{
    std::string return_string(bus->info.device_name);
    char addr[5];
    snprintf(addr, 5, "0x%02X", dev);
    return_string.append(", address ");
    return_string.append(addr);
    return("I2C: " + return_string);
}

std::string MSIGPUController::GetDeviceName()
{
    return(name);
}

void MSIGPUController::SetRGB1(unsigned char red, unsigned char green, unsigned char blue)
{
    MSIGPURegisterWrite(MSI_GPU_REG_R1, red);
    MSIGPURegisterWrite(MSI_GPU_REG_G1, green);
    MSIGPURegisterWrite(MSI_GPU_REG_B1, blue);
}

void MSIGPUController::SetRGB2(unsigned char red, unsigned char green, unsigned char blue)
{
    MSIGPURegisterWrite(MSI_GPU_REG_R2, red);
    MSIGPURegisterWrite(MSI_GPU_REG_G2, green);
    MSIGPURegisterWrite(MSI_GPU_REG_B2, blue);
}

void MSIGPUController::SetRGB3(unsigned char red, unsigned char green, unsigned char blue)
{
    MSIGPURegisterWrite(MSI_GPU_REG_R3, red);
    MSIGPURegisterWrite(MSI_GPU_REG_G3, green);
    MSIGPURegisterWrite(MSI_GPU_REG_B3, blue);
}

void MSIGPUController::SetMode(unsigned char mode)
{
    MSIGPURegisterWrite(MSI_GPU_REG_MODE, mode);
}

void MSIGPUController::Save()
{
    MSIGPURegisterWrite(MSI_GPU_REG_SAVE, 0x01);
}

unsigned char MSIGPUController::MSIGPURegisterRead(unsigned char reg)
{
    return(bus->i2c_smbus_read_byte_data(dev, reg));
}

/*---------------------------------------------------------*\
| NvAPI I2C writes fail transiently (status -1). A dropped  |
| color byte leaves the card on a wrong color until the     |
| next change, so retry immediately: the bus logs each      |
| failed attempt, a recovery is noted here once.            |
\*---------------------------------------------------------*/
#define MSI_GPU_WRITE_RETRIES   2

void MSIGPUController::MSIGPURegisterWrite(unsigned char reg, unsigned char val)
{
    for(int attempt = 0; attempt <= MSI_GPU_WRITE_RETRIES; attempt++)
    {
        if(bus->i2c_smbus_write_byte_data(dev, reg, val) >= 0)
        {
            if(attempt > 0 && !retry_recovery_logged)
            {
                LOG_INFO("[%s] Register 0x%02X write succeeded on retry %d, later recoveries are not logged", name.c_str(), reg, attempt);
                retry_recovery_logged = true;
            }
            return;
        }
    }
}
