/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/LockFreeMutex.hh"
#include "graphics/vulkan/core/VkCommon.hh"

#ifdef SP_DEBUG
    #define HANDLE_POOL_DEBUG_UNFREED_HANDLES
#endif

namespace sp::vulkan {
    template<typename HandleType>
    class SharedHandle {
    public:
        struct Data {
            struct PoolRef {
                vector<SharedHandle<HandleType>> *freeList;
                LockFreeMutex *freeMutex;

#ifdef HANDLE_POOL_DEBUG_UNFREED_HANDLES
                shared_ptr<std::atomic_flag> destroyed;
#endif
            };

            std::atomic_size_t refCount;
            PoolRef pool;

            Data(PoolRef &&pool) : refCount(1), pool(std::move(pool)) {}
        };

        SharedHandle() {}
        SharedHandle(std::nullptr_t) {}

        SharedHandle(SharedHandle<HandleType> &&other) noexcept : handle(std::move(other.handle)), data(other.data) {
            other.data = nullptr;
        }

        SharedHandle(const SharedHandle<HandleType> &other) noexcept : handle(other.handle), data(other.data) {
            if (data) data->refCount++;
        }

        explicit SharedHandle(HandleType &&handle, typename Data::PoolRef &&pool)
            : handle(std::move(handle)), data(new Data(std::move(pool))) {}

        ~SharedHandle() {
            Destroy();
        }

        void Reset(const SharedHandle<HandleType> &newValue) {
            Destroy();
            handle = newValue.handle;
            data = newValue.data;
            if (data) data->refCount++;
        }

        SharedHandle<HandleType> &operator=(const SharedHandle<HandleType> &other) {
            Reset(other);
            return *this;
        }

        operator HandleType() {
            return handle;
        }

        explicit operator bool() const {
            return !!data;
        }

        HandleType &Get() {
            return handle;
        }

    private:
        void Destroy() {
            if (!data) return; // SharedHandle doesn't own a handle

#ifdef HANDLE_POOL_DEBUG_UNFREED_HANDLES
            Assert(!data->pool.destroyed->test(), "pool destroyed before this allocation");
#endif
            auto uses = data->refCount.fetch_sub(1);
            if (uses == 0) {
                delete data;
                data = nullptr;
            } else if (uses == 1) {
                std::unique_lock lock(*data->pool.freeMutex);
                data->pool.freeList->push_back(std::move(*this));
            }
        }

        HandleType handle = {};
        Data *data = nullptr;

        template<typename>
        friend class HandlePool;
    };

    template<typename HandleType>
    class HandlePool {
    public:
        HandlePool(std::function<HandleType()> createObject,
            std::function<void(HandleType)> destroyObject,
            std::function<void(HandleType)> resetObject = {})
            : createObject(createObject), destroyObject(destroyObject), resetObject(resetObject),
              destroyed(make_shared<std::atomic_flag>()) {}

        HandlePool() : destroyed(make_shared<std::atomic_flag>()) {}

        HandlePool(HandlePool<HandleType> &&) = default;

        ~HandlePool() {
            std::unique_lock lock(freeMutex);
            size_t freeCount = freeObjects.size();
            for (auto &object : freeObjects) {
                destroyObject(object);
            }
            freeObjects.clear();

#ifdef HANDLE_POOL_DEBUG_UNFREED_HANDLES
            if (freeCount != totalObjects || destroyed.use_count() > 1) {
                Errorf("[HandlePool] %d handles weren't freed before the pool (%d created, %d freed)",
                    destroyed.use_count() - 1,
                    totalObjects,
                    freeCount);
            }
#else
            if (freeCount != totalObjects) {
                Errorf("[HandlePool] some handles weren't freed before the pool (%d created, %d freed)",
                    totalObjects,
                    freeCount);
            }
#endif
            destroyed->test_and_set();
        }

        SharedHandle<HandleType> Get() {
            {
                std::unique_lock lock(freeMutex);
                if (!freeObjects.empty()) {
                    SharedHandle<HandleType> sharedHandle = std::move(freeObjects.back());
                    freeObjects.pop_back();
                    lock.unlock();
                    sharedHandle.data->refCount++;
                    if (resetObject) resetObject(sharedHandle);
                    return sharedHandle;
                }
            }

            HandleType object = createObject();
            totalObjects++;
#ifdef HANDLE_POOL_DEBUG_UNFREED_HANDLES
            return SharedHandle<HandleType>{std::move(object), {&freeObjects, &freeMutex, destroyed}};
#else
            return SharedHandle<HandleType>{std::move(object), {&freeObjects, &freeMutex}};
#endif
        }

    private:
        std::function<HandleType()> createObject;
        std::function<void(HandleType)> destroyObject, resetObject;

        LockFreeMutex freeMutex;
        vector<SharedHandle<HandleType>> freeObjects;
        size_t totalObjects = 0;

        shared_ptr<std::atomic_flag> destroyed;
    };
} // namespace sp::vulkan
