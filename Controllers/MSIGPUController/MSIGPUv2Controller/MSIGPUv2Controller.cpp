/*---------------------------------------------------------*\
| MSIGPUv2Controller.cpp                                    |
|                                                           |
|   Driver for MSI V2 GPU (ITE9)                            |
|                                                           |
|   Wojciech Lazarski                           03 Jan 2023 |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <cstring>
#include <chrono>
#include "MSIGPUv2Controller.h"
#include "LogManager.h"

using namespace std::chrono_literals;

MSIGPUv2Controller::MSIGPUv2Controller(i2c_smbus_interface* bus, msi_gpu_dev_id dev, std::string dev_name)
{
    this->bus   = bus;
    this->dev   = dev;
    this->name  = dev_name;

    LOG_WARNING("[MSI GPU v2] %s (%s) initial state: %s", name.c_str(), GetDeviceLocation().c_str(), ReadStateString().c_str());
}

MSIGPUv2Controller::~MSIGPUv2Controller()
{

}

std::string MSIGPUv2Controller::GetDeviceLocation()
{
    std::string return_string(bus->info.device_name);
    char addr[5];
    snprintf(addr, 5, "0x%02X", dev);
    return_string.append(", address ");
    return_string.append(addr);
    return("I2C: " + return_string);
}

std::string MSIGPUv2Controller::GetDeviceName()
{
    return(name);
}

void MSIGPUv2Controller::SetRGB1(unsigned char red, unsigned char green, unsigned char blue)
{
    MSIGPURegisterWrite(MSI_GPU_V2_REG_R1, red);
    MSIGPURegisterWrite(MSI_GPU_V2_REG_G1, green);
    MSIGPURegisterWrite(MSI_GPU_V2_REG_B1, blue);
}

void MSIGPUv2Controller::SetRGB1V2(unsigned char red, unsigned char green, unsigned char blue)
{
    unsigned char buffer[3];
    buffer[2]=red;
    buffer[1]=green;
    buffer[0]=blue;
    MSIGPUBlockWrite(MSI_GPU_V2_REG_COLOR_BLOCK1_BASE, &buffer[0], sizeof(buffer));
}

void MSIGPUv2Controller::SetRGB2V2(unsigned char red, unsigned char green, unsigned char blue)
{
    unsigned char buffer[3];
    buffer[2]=red;
    buffer[1]=green;
    buffer[0]=blue;
    MSIGPUBlockWrite(MSI_GPU_V2_REG_COLOR_BLOCK2_BASE, &buffer[0], sizeof(buffer));
}

void MSIGPUv2Controller::SetRGB3V2(unsigned char red, unsigned char green, unsigned char blue)
{
    unsigned char buffer[3];
    buffer[2]=red;
    buffer[1]=green;
    buffer[0]=blue;
    MSIGPUBlockWrite(MSI_GPU_V2_REG_COLOR_BLOCK3_BASE, &buffer[0], sizeof(buffer));
}

void MSIGPUv2Controller::SetMode(unsigned char mode)
{
    MSIGPURegisterWrite(MSI_GPU_V2_REG_MODE, mode);
}

void MSIGPUv2Controller::Save()
{
    MSIGPURegisterWrite(MSI_GPU_V2_REG_SAVE, 0x00);
}

/*---------------------------------------------------------*\
| Mode, brightness and R1/G1/B1 as the card reports them,   |
| "err" for a register whose read failed                    |
\*---------------------------------------------------------*/
std::string MSIGPUv2Controller::ReadStateString()
{
    static const unsigned char regs[5] = { MSI_GPU_V2_REG_MODE, MSI_GPU_V2_REG_BRIGHTNESS, MSI_GPU_V2_REG_R1, MSI_GPU_V2_REG_G1, MSI_GPU_V2_REG_B1 };
    char                       vals[5][5];

    for(int idx = 0; idx < 5; idx++)
    {
        s32 val = bus->i2c_smbus_read_byte_data(dev, regs[idx]);

        if(val < 0)
        {
            snprintf(vals[idx], sizeof(vals[idx]), "err");
        }
        else
        {
            snprintf(vals[idx], sizeof(vals[idx]), "0x%02X", val & 0xFF);
        }
    }

    char buf[96];
    snprintf(buf, sizeof(buf), "mode %s brightness %s rgb %s/%s/%s", vals[0], vals[1], vals[2], vals[3], vals[4]);
    return(buf);
}

unsigned char MSIGPUv2Controller::MSIGPURegisterRead(unsigned char reg)
{
    return bus->i2c_smbus_read_byte_data(dev, reg);
}

void MSIGPUv2Controller::MSIGPURegisterWrite(unsigned char reg, unsigned char val)
{
    bus->i2c_smbus_write_byte_data(dev, reg, val);
    std::this_thread::sleep_for(20ms);
}

void MSIGPUv2Controller::MSIGPUBlockWrite(unsigned char reg, unsigned char *val, unsigned char len)
{
    bus->i2c_smbus_interface::i2c_smbus_write_i2c_block_data(dev, reg, len, val);
    std::this_thread::sleep_for(20ms);
}
