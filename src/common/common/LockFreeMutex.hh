/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Common.hh"

#include <atomic>
#include <cstddef>

static_assert(ATOMIC_INT_LOCK_FREE == 2, "std::atomic_int is not lock-free");

namespace sp {
    // Implements the requirements for SharedMutex: https://en.cppreference.com/w/cpp/named_req/SharedMutex
    class LockFreeMutex : public NonCopyable {
    public:
        // Read locks
        void lock_shared();
        bool try_lock_shared(bool yieldWaiting = true);
        void unlock_shared();

        // Write locks
        void lock();
        bool try_lock();
        void unlock();

    private:
        static const uint32_t LOCK_STATE_FREE = 0;
        static const uint32_t LOCK_STATE_EXCLUSIVE_LOCKED = UINT32_MAX;

        static const size_t SPINLOCK_RETRY_YIELD = 10;

        std::atomic_uint32_t lockState;
        std::atomic_bool exclusiveWaiting;
    };
} // namespace sp
