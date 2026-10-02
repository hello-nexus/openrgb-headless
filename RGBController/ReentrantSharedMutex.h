/*---------------------------------------------------------*\
| ReentrantSharedMutex.h                                    |
|                                                           |
|   A shared mutex a thread may re-acquire while it already |
|   holds it. Windows SRW locks (std::shared_mutex) deadlock |
|   when a thread takes a shared lock it already holds while |
|   a writer is queued, which happens when a controller's    |
|   DeviceUpdateLEDs() calls a locking getter such as        |
|   GetActiveMode() under the device thread's shared lock    |
|   and a network UpdateLEDs waits for the exclusive lock.   |
|                                                           |
|   Upgrading a held shared lock to exclusive still blocks,  |
|   exactly as with std::shared_mutex.                      |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <shared_mutex>
#include <vector>

class ReentrantSharedMutex
{
public:
    void lock()
    {
        Hold& hold = Find();

        if(hold.mode == MODE_EXCLUSIVE)
        {
            hold.exclusive++;
            return;
        }

        mutex.lock();
        hold.mode       = MODE_EXCLUSIVE;
        hold.exclusive  = 1;
    }

    void unlock()
    {
        Hold& hold = Find();

        hold.exclusive--;
        Release(hold);
    }

    void lock_shared()
    {
        Hold& hold = Find();

        if(hold.mode != MODE_NONE)
        {
            hold.shared++;
            return;
        }

        mutex.lock_shared();
        hold.mode   = MODE_SHARED;
        hold.shared = 1;
    }

    void unlock_shared()
    {
        Hold& hold = Find();

        hold.shared--;
        Release(hold);
    }

private:
    enum
    {
        MODE_NONE,
        MODE_SHARED,
        MODE_EXCLUSIVE
    };

    struct Hold
    {
        const ReentrantSharedMutex* owner;
        int                         mode;
        int                         shared;
        int                         exclusive;
    };

    std::shared_mutex mutex;

    /*-----------------------------------------------------*\
    | Per-thread record of the mutexes this thread holds    |
    \*-----------------------------------------------------*/
    static std::vector<Hold>& Holds()
    {
        thread_local std::vector<Hold> holds;
        return(holds);
    }

    Hold& Find()
    {
        std::vector<Hold>& holds = Holds();

        for(Hold& hold : holds)
        {
            if(hold.owner == this)
            {
                return(hold);
            }
        }

        holds.push_back({ this, MODE_NONE, 0, 0 });
        return(holds.back());
    }

    void Release(Hold& hold)
    {
        if(hold.shared > 0 || hold.exclusive > 0)
        {
            return;
        }

        if(hold.mode == MODE_EXCLUSIVE)
        {
            mutex.unlock();
        }
        else if(hold.mode == MODE_SHARED)
        {
            mutex.unlock_shared();
        }

        std::vector<Hold>& holds = Holds();

        for(std::size_t idx = 0; idx < holds.size(); idx++)
        {
            if(holds[idx].owner == this)
            {
                holds.erase(holds.begin() + idx);
                break;
            }
        }
    }
};
