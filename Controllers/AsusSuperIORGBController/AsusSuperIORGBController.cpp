/*---------------------------------------------------------*\
| AsusSuperIORGBController.cpp                              |
|                                                           |
|   Driver for the 12V RGB header that ASUS boards without  |
|   an Aura chip drive through the Nuvoton Super I/O        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <memory>
#include "AsusSuperIORGBController.h"
#include "LogManager.h"
#include "super_io.h"

#ifdef _WIN32
#include <Windows.h>
#endif

/*---------------------------------------------------------*\
| The Super I/O is shared with fan control and monitoring.  |
| On Windows, hold the ASUS AI Suite locks (only when they  |
| already exist: they are opened, never created) and the    |
| ISA bus mutex LibreHardwareMonitor uses, for as long as   |
| the chip is in configuration mode. Every wait is          |
| bounded; on timeout the access is skipped.                |
\*---------------------------------------------------------*/
#define ASUS_SIO_LOCK_TIMEOUT_MS    250
#define ASUS_SIO_LOCK_ATTEMPTS      3
#define ASUS_SIO_DEDUPE_WINDOW_MS   1000

#ifdef _WIN32
/*---------------------------------------------------------*\
| An open error does not clear up on retry; log it once     |
\*---------------------------------------------------------*/
static bool asus_sio_open_error_logged = false;

static void AsusSuperIOLogOpenError(const char* lock_name, DWORD error)
{
    if(!asus_sio_open_error_logged)
    {
        asus_sio_open_error_logged = true;
        LOG_WARNING("[ASUS Super IO] Could not open %s (error %lu), skipping Super IO access", lock_name, error);
    }
}
#endif

class AsusSuperIOLock
{
public:
    AsusSuperIOLock()
    {
        open_error = false;

#ifdef _WIN32
        acquired = false;

        /*-------------------------------------------------*\
        | Only ERROR_FILE_NOT_FOUND means the lock is       |
        | absent; any other open failure skips the access   |
        \*-------------------------------------------------*/
        asus_sio_event = OpenEventA(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, "Global\\ASUS_SIO_MUTEX");
        if(asus_sio_event == NULL)
        {
            DWORD error = GetLastError();
            if(error != ERROR_FILE_NOT_FOUND)
            {
                AsusSuperIOLogOpenError("Global\\ASUS_SIO_MUTEX", error);
                open_error = true;
                return;
            }
        }
        if(asus_sio_event != NULL)
        {
            if(WaitForSingleObject(asus_sio_event, ASUS_SIO_LOCK_TIMEOUT_MS) != WAIT_OBJECT_0)
            {
                LOG_WARNING("[ASUS Super IO] Timed out waiting for Global\\ASUS_SIO_MUTEX, skipping access");
                CloseHandle(asus_sio_event);
                asus_sio_event = NULL;
                return;
            }
        }

        asus_isa_mutex = OpenMutexA(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, "Global\\ASUS_ISABUS_MUTEX");
        if(asus_isa_mutex == NULL)
        {
            DWORD error = GetLastError();
            if(error != ERROR_FILE_NOT_FOUND)
            {
                AsusSuperIOLogOpenError("Global\\ASUS_ISABUS_MUTEX", error);
                open_error = true;
                Release();
                return;
            }
        }
        if(asus_isa_mutex != NULL)
        {
            DWORD status = WaitForSingleObject(asus_isa_mutex, ASUS_SIO_LOCK_TIMEOUT_MS);
            if(status != WAIT_OBJECT_0 && status != WAIT_ABANDONED)
            {
                LOG_WARNING("[ASUS Super IO] Timed out waiting for Global\\ASUS_ISABUS_MUTEX, skipping access");
                CloseHandle(asus_isa_mutex);
                asus_isa_mutex = NULL;
                Release();
                return;
            }
        }

        isa_mutex = CreateMutexA(NULL, FALSE, "Global\\Access_ISABUS.HTP.Method");
        if(isa_mutex == NULL)
        {
            AsusSuperIOLogOpenError("Global\\Access_ISABUS.HTP.Method", GetLastError());
            open_error = true;
            Release();
            return;
        }
        DWORD status = WaitForSingleObject(isa_mutex, ASUS_SIO_LOCK_TIMEOUT_MS);
        if(status != WAIT_OBJECT_0 && status != WAIT_ABANDONED)
        {
            LOG_WARNING("[ASUS Super IO] Timed out waiting for Global\\Access_ISABUS.HTP.Method, skipping access");
            CloseHandle(isa_mutex);
            isa_mutex = NULL;
            Release();
            return;
        }

        acquired = true;
#else
        acquired = true;
#endif
    }

    ~AsusSuperIOLock()
    {
#ifdef _WIN32
        Release();
#endif
    }

    bool Acquired() const
    {
        return(acquired);
    }

    bool OpenError() const
    {
        return(open_error);
    }

private:
    bool    acquired;
    bool    open_error;

#ifdef _WIN32
    HANDLE  asus_sio_event  = NULL;
    HANDLE  asus_isa_mutex  = NULL;
    HANDLE  isa_mutex       = NULL;

    /*-----------------------------------------------------*\
    | Each handle is non-NULL only while it is held         |
    \*-----------------------------------------------------*/
    void Release()
    {
        if(isa_mutex != NULL)
        {
            ReleaseMutex(isa_mutex);
            CloseHandle(isa_mutex);
            isa_mutex = NULL;
        }
        if(asus_isa_mutex != NULL)
        {
            ReleaseMutex(asus_isa_mutex);
            CloseHandle(asus_isa_mutex);
            asus_isa_mutex = NULL;
        }
        if(asus_sio_event != NULL)
        {
            /*---------------------------------------------*\
            | Auto-reset event: signaled means free         |
            \*---------------------------------------------*/
            SetEvent(asus_sio_event);
            CloseHandle(asus_sio_event);
            asus_sio_event = NULL;
        }
        acquired = false;
    }
#endif
};

/*---------------------------------------------------------*\
| Each attempt blocks on the lock waits; only a wait        |
| timeout is retried. NULL when the locks were not taken.   |
\*---------------------------------------------------------*/
static std::unique_ptr<AsusSuperIOLock> AsusSuperIOAcquire()
{
    for(int attempt = 0; attempt < ASUS_SIO_LOCK_ATTEMPTS; attempt++)
    {
        std::unique_ptr<AsusSuperIOLock> lock(new AsusSuperIOLock());

        if(lock->Acquired())
        {
            return(lock);
        }

        if(lock->OpenError())
        {
            return(nullptr);
        }
    }

    LOG_WARNING("[ASUS Super IO] Could not take the Super IO locks after %d attempts", ASUS_SIO_LOCK_ATTEMPTS);
    return(nullptr);
}

/*---------------------------------------------------------*\
| The super_io layer has no bare index-port write; an inb   |
| of the key writes it to the index port, as superio_enter  |
| does for 0x87                                             |
\*---------------------------------------------------------*/
static void AsusSuperIOExit(int sioaddr)
{
    superio_inb(sioaddr, ASUS_SIO_EXIT_KEY);
}

/*---------------------------------------------------------*\
| Register order of the vendor software                     |
\*---------------------------------------------------------*/
static const unsigned char led_regs[] =
{
    0xF0, 0xF4, 0xF8, 0xF1, 0xF5, 0xF9, 0xF2, 0xF6, 0xFA, 0xF3, 0xF7, 0xFB
};

static const unsigned char param_regs[] =
{
    0xF8, 0xF3, 0xF4, 0xF9, 0xFA, 0xF0, 0xF1, 0xF2, 0xF5, 0xF6, 0xF7, 0xFB
};

/*---------------------------------------------------------*\
| LDN 0x15 sets as captured. Index: register - 0xF0.        |
| 0xF8 (speed) is filled in per write.                      |
\*---------------------------------------------------------*/
static const unsigned char params_static[12]   = { 0xFF, 0x7F, 0x00, 0x06, 0x05, 0x17, 0x00, 0x0F, 0x00, 0x06, 0x05, 0xFF };
static const unsigned char params_strobing[12] = { 0xFF, 0x7F, 0x00, 0x02, 0x0F, 0x17, 0x00, 0x0F, 0x00, 0xFF, 0xFF, 0xFF };
static const unsigned char params_fading[12]   = { 0xFF, 0x7F, 0x00, 0x03, 0x03, 0x17, 0x00, 0x0F, 0x00, 0x06, 0x05, 0xFF };

static const unsigned char speed_blinking[3]   = { 0x10, 0x08, 0x01 };
static const unsigned char speed_strobing[3]   = { 0x10, 0x0A, 0x07 };

AsusSuperIORGBController::AsusSuperIORGBController(int sioaddr, std::string dev_name)
{
    this->sioaddr   = sioaddr;
    name            = dev_name;
    last_valid      = false;
    last_effect     = 0;
    last_speed      = 0;
    last_r4         = 0;
    last_g4         = 0;
    last_b4         = 0;
}

AsusSuperIORGBController::~AsusSuperIORGBController()
{

}

int AsusSuperIORGBController::ReadChipID(int sioaddr)
{
    std::unique_ptr<AsusSuperIOLock> lock = AsusSuperIOAcquire();

    if(!lock)
    {
        return(-1);
    }

    superio_enter(sioaddr);

    int hi = superio_inb(sioaddr, SIO_REG_DEVID);
    int lo = superio_inb(sioaddr, SIO_REG_DEVID + 1);

    AsusSuperIOExit(sioaddr);

    if(hi < 0 || lo < 0)
    {
        return(-1);
    }

    return((hi << 8) | lo);
}

std::string AsusSuperIORGBController::GetDeviceLocation()
{
    char hex[12];
    snprintf(hex, sizeof(hex), "0x%X", sioaddr);
    return("SIO: " + std::string(hex));
}

std::string AsusSuperIORGBController::GetDeviceName()
{
    return(name);
}

void AsusSuperIORGBController::Apply(unsigned int effect, unsigned int speed, unsigned char red, unsigned char green, unsigned char blue)
{
    /*-----------------------------------------------------*\
    | The chip takes 4 bits per channel                     |
    \*-----------------------------------------------------*/
    unsigned char r4 = red   >> 4;
    unsigned char g4 = green >> 4;
    unsigned char b4 = blue  >> 4;

    if(effect == ASUS_SIO_EFFECT_OFF)
    {
        speed   = 0;
        r4      = 0;
        g4      = 0;
        b4      = 0;
    }

    /*-----------------------------------------------------*\
    | The chip is shared with fan control: skip an          |
    | identical write only shortly after the last one, so a |
    | later re-apply (resume, vendor tool) reaches the chip |
    \*-----------------------------------------------------*/
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

    bool same   = last_valid && effect == last_effect && speed == last_speed && r4 == last_r4 && g4 == last_g4 && b4 == last_b4;
    bool steady = effect == ASUS_SIO_EFFECT_STATIC || effect == ASUS_SIO_EFFECT_OFF;

    /*-----------------------------------------------------*\
    | A rewrite restarts an animated mode, so those skip an |
    | identical write with no time window                   |
    \*-----------------------------------------------------*/
    if(same && (!steady || now - last_write < std::chrono::milliseconds(ASUS_SIO_DEDUPE_WINDOW_MS)))
    {
        return;
    }

    last_valid = false;

    if(Write(effect, speed, r4, g4, b4))
    {
        last_valid  = true;
        last_write  = now;
        last_effect = effect;
        last_speed  = speed;
        last_r4     = r4;
        last_g4     = g4;
        last_b4     = b4;
    }
}

bool AsusSuperIORGBController::Write(unsigned int effect, unsigned int speed, unsigned char r4, unsigned char g4, unsigned char b4)
{
    unsigned char       mode;
    const unsigned char* params     = params_static;
    unsigned char       speed_val   = 0x10;

    if(speed > ASUS_SIO_SPEED_FAST)
    {
        speed = ASUS_SIO_SPEED_FAST;
    }

    switch(effect)
    {
        case ASUS_SIO_EFFECT_OFF:
            mode        = ASUS_SIO_MODE_OFF;
            break;

        case ASUS_SIO_EFFECT_BLINKING:
            mode        = ASUS_SIO_MODE_BLINKING;
            speed_val   = speed_blinking[speed];
            break;

        case ASUS_SIO_EFFECT_STROBING:
            mode        = ASUS_SIO_MODE_CYCLE;
            params      = params_strobing;
            speed_val   = speed_strobing[speed];
            break;

        case ASUS_SIO_EFFECT_FADING:
            mode        = ASUS_SIO_MODE_CYCLE;
            params      = params_fading;
            break;

        case ASUS_SIO_EFFECT_STATIC:
        default:
            mode        = ASUS_SIO_MODE_STATIC;
            break;
    }

    /*-----------------------------------------------------*\
    | The same color in all 8 stages (low nibble odd stage, |
    | high nibble even stage)                               |
    \*-----------------------------------------------------*/
    unsigned char r_byte = (unsigned char)((r4 << 4) | r4);
    unsigned char g_byte = (unsigned char)((g4 << 4) | g4);
    unsigned char b_byte = (unsigned char)((b4 << 4) | b4);

    std::unique_ptr<AsusSuperIOLock> lock = AsusSuperIOAcquire();

    if(!lock)
    {
        return(false);
    }

    superio_enter(sioaddr);

    /*-----------------------------------------------------*\
    | LDN 0x12 register 0xED: read and write back unchanged |
    \*-----------------------------------------------------*/
    superio_outb(sioaddr, SIO_REG_LOGDEV, ASUS_SIO_LOGDEV_PREFIX);

    int prefix = superio_inb(sioaddr, ASUS_SIO_REG_PREFIX);

    if(prefix < 0)
    {
        AsusSuperIOExit(sioaddr);
        LOG_WARNING("[ASUS Super IO] Read of LDN 0x12 register 0xED failed, skipping write");
        return(false);
    }

    superio_outb(sioaddr, ASUS_SIO_REG_PREFIX, prefix);

    /*-----------------------------------------------------*\
    | LDN 0x13: mode, then the color stages                 |
    \*-----------------------------------------------------*/
    superio_outb(sioaddr, SIO_REG_LOGDEV, ASUS_SIO_LOGDEV_LED);
    superio_inb(sioaddr, ASUS_SIO_REG_MODE);
    superio_outb(sioaddr, ASUS_SIO_REG_MODE, mode);

    if(mode != ASUS_SIO_MODE_OFF)
    {
        for(unsigned int i = 0; i < sizeof(led_regs); i++)
        {
            unsigned char reg = led_regs[i];
            unsigned char val;

            if(reg <= 0xF3)
            {
                val = r_byte;
            }
            else if(reg <= 0xF7)
            {
                val = g_byte;
            }
            else
            {
                val = b_byte;
            }

            superio_outb(sioaddr, reg, val);
        }

        /*-------------------------------------------------*\
        | One stage                                         |
        \*-------------------------------------------------*/
        superio_outb(sioaddr, ASUS_SIO_REG_STAGES, 0x01);

        /*-------------------------------------------------*\
        | LDN 0x15: the per-mode parameter set              |
        \*-------------------------------------------------*/
        superio_outb(sioaddr, SIO_REG_LOGDEV, ASUS_SIO_LOGDEV_PARAMS);

        for(unsigned int i = 0; i < sizeof(param_regs); i++)
        {
            unsigned char reg = param_regs[i];
            unsigned char val = (reg == 0xF8) ? speed_val : params[reg - 0xF0];

            superio_outb(sioaddr, reg, val);
        }
    }

    AsusSuperIOExit(sioaddr);

    return(true);
}
