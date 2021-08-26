#include "LockFreeMutex.hh"

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
        if (current != LOCK_STATE_EXCLUSIVE_LOCKED) {
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
        if (current == LOCK_STATE_FREE || current == LOCK_STATE_EXCLUSIVE_LOCKED) {
            throw std::runtime_error("LockFreeMutex::unlock_shared() called without active shared lock");
        }
        lockState--;
    }

    void LockFreeMutex::lock() {
        size_t retry = 0;
        while (true) {
            if (try_lock()) return;

            if (retry++ > SPINLOCK_RETRY_YIELD) {
                retry = 0;
                std::this_thread::yield();
            }
        }
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
        if (current == LOCK_STATE_EXCLUSIVE_LOCKED) {
            if (!lockState.compare_exchange_strong(current, LOCK_STATE_FREE)) {
                throw std::runtime_error("LockFreeMutex::unlock() lockState changed unexpectedly");
            }
        } else {
            throw std::runtime_error("LockFreeMutex::unlock() called without active exclusive lock");
        }
    }
} // namespace sp
