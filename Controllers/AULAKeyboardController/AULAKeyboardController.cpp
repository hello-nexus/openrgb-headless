/*---------------------------------------------------------*\
| AULAKeyboardController.cpp                                |
|                                                           |
|   Driver for AULA F75 Max keyboard (Sonix)                |
|                                                           |
|   Nexus                                       05 Jul 2026 |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <cstring>
#include "AULAKeyboardController.h"
#include "StringUtils.h"

AULAKeyboardController::AULAKeyboardController(hid_device* dev_handle, const char* path, std::string dev_name)
{
    dev         = dev_handle;
    location    = path;
    name        = dev_name;
}

AULAKeyboardController::~AULAKeyboardController()
{
    hid_close(dev);
}

std::string AULAKeyboardController::GetDeviceLocation()
{
    return("HID: " + location);
}

std::string AULAKeyboardController::GetNameString()
{
    return(name);
}

std::string AULAKeyboardController::GetSerialString()
{
    wchar_t serial_string[128];
    int ret = hid_get_serial_number_string(dev, serial_string, 128);

    if(ret != 0)
    {
        return("");
    }

    return(StringUtils::wstring_to_string(serial_string));
}

/*-------------------------------------------------------------------------------------------------*\
| Send a full LED frame: Begin -> N data chunks -> End.                                              |
|                                                                                                   |
| quad_data is a "quad stream" of [hw_index, R, G, B] tuples (4 bytes per LED).  It is split into    |
| 64-byte chunks, the last of which is zero-padded to 64 bytes.                                      |
\*-------------------------------------------------------------------------------------------------*/

void AULAKeyboardController::SetKeyboardColors
    (
    unsigned char *     quad_data,
    unsigned int        size
    )
{
    unsigned int    offset = 0;

    SendKeyboardBegin();

    while(offset < size)
    {
        unsigned char   chunk[AULA_KB_DATA_SIZE];
        unsigned int    chunk_size = size - offset;

        if(chunk_size > AULA_KB_DATA_SIZE)
        {
            chunk_size = AULA_KB_DATA_SIZE;
        }

        /*-----------------------------------------------------*\
        | Zero-pad the (last) chunk to a full 64 bytes          |
        \*-----------------------------------------------------*/
        memset(chunk, 0x00, sizeof(chunk));
        memcpy(chunk, &quad_data[offset], chunk_size);

        SendKeyboardData(chunk);

        offset += chunk_size;
    }

    SendKeyboardEnd();
}

/*-------------------------------------------------------------------------------------------------*\
| Private packet sending functions.                                                                 |
|   Note: These devices use no checksum.  hid_read is only issued after Begin and End.              |
\*-------------------------------------------------------------------------------------------------*/

void AULAKeyboardController::SendKeyboardBegin()
{
    unsigned char usb_buf[AULA_KB_REPORT_SIZE];

    /*-----------------------------------------------------*\
    | Zero out buffer (buf[0] is the 0x00 report id)        |
    \*-----------------------------------------------------*/
    memset(usb_buf, 0x00, sizeof(usb_buf));

    /*-----------------------------------------------------*\
    | Set up frame Begin packet                             |
    \*-----------------------------------------------------*/
    usb_buf[0x01]           = 0x04;
    usb_buf[0x02]           = 0x20;
    usb_buf[0x09]           = 0x08;

    /*-----------------------------------------------------*\
    | Send packet                                           |
    \*-----------------------------------------------------*/
    hid_write(dev, usb_buf, AULA_KB_REPORT_SIZE);
    hid_read(dev, usb_buf, AULA_KB_REPORT_SIZE);
}

void AULAKeyboardController::SendKeyboardData
    (
    unsigned char *     data
    )
{
    unsigned char usb_buf[AULA_KB_REPORT_SIZE];

    /*-----------------------------------------------------*\
    | Zero out buffer (buf[0] is the 0x00 report id)        |
    \*-----------------------------------------------------*/
    memset(usb_buf, 0x00, sizeof(usb_buf));

    /*-----------------------------------------------------*\
    | Copy in the 64-byte data chunk                        |
    \*-----------------------------------------------------*/
    memcpy(&usb_buf[0x01], data, AULA_KB_DATA_SIZE);

    /*-----------------------------------------------------*\
    | Send packet (no read for data chunks)                 |
    \*-----------------------------------------------------*/
    hid_write(dev, usb_buf, AULA_KB_REPORT_SIZE);
}

void AULAKeyboardController::SendKeyboardEnd()
{
    unsigned char usb_buf[AULA_KB_REPORT_SIZE];

    /*-----------------------------------------------------*\
    | Send an empty packet first                            |
    \*-----------------------------------------------------*/
    memset(usb_buf, 0x00, sizeof(usb_buf));
    hid_write(dev, usb_buf, AULA_KB_REPORT_SIZE);

    /*-----------------------------------------------------*\
    | Set up frame End packet                               |
    \*-----------------------------------------------------*/
    memset(usb_buf, 0x00, sizeof(usb_buf));
    usb_buf[0x01]           = 0x04;
    usb_buf[0x02]           = 0x02;

    /*-----------------------------------------------------*\
    | Send packet                                           |
    \*-----------------------------------------------------*/
    hid_write(dev, usb_buf, AULA_KB_REPORT_SIZE);
    hid_read(dev, usb_buf, AULA_KB_REPORT_SIZE);
}
