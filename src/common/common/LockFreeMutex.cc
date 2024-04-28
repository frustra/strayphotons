/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "LockFreeMutex.hh"

#include "common/Logging.hh"

#include <thread>

namespace sp {
    void LockFreeMutex::lock_shared() {
        size_t retry = 0;
        while (true) {
            if (try_lock_shared()) return;

            if (retry++ > SPINLOCK_RETRY_YIELD) {
                retry = 0;
                std::this_thread::yield();
            }
        }
    }

    bool LockFreeMutex::try_lock_shared() {
        uint32_t current = lockState;
        if (current != LOCK_STATE_EXCLUSIVE_LOCKED && !exclusiveWaiting) {
            uint32_t next = current + 1;
            if (lockState.compare_exchange_weak(current, next)) {
                // Lock aquired
                return true;
            }
        }

        return false;
    }

    void LockFreeMutex::unlock_shared() {
        uint32_t current = lockState;
        Assert(current != LOCK_STATE_FREE, "LockFreeMutex::unlock_shared() called without active shared lock");
        Assert(current != LOCK_STATE_EXCLUSIVE_LOCKED,
            "LockFreeMutex::unlock_shared() called while exclusive lock held");
        lockState--;
    }

    void LockFreeMutex::lock() {
        size_t retry = 0;
        while (true) {
            bool current = exclusiveWaiting;
            if (!current && exclusiveWaiting.compare_exchange_weak(current, true)) break;

            if (retry++ > SPINLOCK_RETRY_YIELD) {
                retry = 0;
                std::this_thread::yield();
            }
        }
        while (true) {
            if (try_lock()) break;

            if (retry++ > SPINLOCK_RETRY_YIELD) {
                retry = 0;
                std::this_thread::yield();
            }
        }
        bool current = exclusiveWaiting;
        Assert(current, "LockFreeMutex::lock() exclusiveWaiting changed unexpectedly");
        bool success = exclusiveWaiting.compare_exchange_strong(current, false);
        Assert(success, "LockFreeMutex::lock() exclusiveWaiting change failed");
    }

    bool LockFreeMutex::try_lock() {
        uint32_t current = lockState;
        if (current == LOCK_STATE_FREE) {
            if (lockState.compare_exchange_weak(current, LOCK_STATE_EXCLUSIVE_LOCKED)) {
                // Lock aquired
                return true;
            }
        }

        return false;
    }

    void LockFreeMutex::unlock() {
        uint32_t current = lockState;
        Assert(current == LOCK_STATE_EXCLUSIVE_LOCKED, "LockFreeMutex::unlock() called without active exclusive lock");
        bool success = lockState.compare_exchange_strong(current, LOCK_STATE_FREE);
        Assert(success, "LockFreeMutex::unlock() lockState change failed");
    }
} // namespace sp
