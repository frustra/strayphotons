/*
 * Stray Photons - Copyright (C) 2026 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Tracing.hh"
#include "strayphotons/LockFreeMutex.hh"
#include "strayphotons/Logging.hh"
#include "strayphotons/Utility.hh"

#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <memory_resource>
#include <mutex>

namespace sp {
    template<typename T>
    class ObjectPool : public NonMoveable {
    public:
        ObjectPool(std::function<void(T *)> &&constructObject, std::function<void(T &)> &&resetObject = {})
            : memoryPool(), allocator(&memoryPool), constructObject(std::move(constructObject)),
              resetObject(std::move(resetObject)) {}
        ~ObjectPool() {
            std::unique_lock lock(freeMutex);
            Assertf(freeObjects.size() == objectCount,
                "ObjectPool destroyed with %u/%u objects still active",
                objectCount - freeObjects.size(),
                objectCount.load());
            for (T *obj : freeObjects) {
                obj->~T();
                allocator.deallocate_object(obj);
            }
            freeObjects.clear();
            objectCount = 0;
        }

        void Reserve(size_t count) {
            std::unique_lock lock(freeMutex);
            while (freeObjects.size() < count) {
                T *newObj = allocator.template allocate_object<T>();
                if (constructObject) constructObject(newObj);
                freeObjects.emplace_back(newObj);
                objectCount++;
            }
        }

        std::shared_ptr<T> Get() {
            T *available = nullptr;
            {
                std::unique_lock lock(freeMutex);
                if (!freeObjects.empty()) {
                    available = freeObjects.front();
                    freeObjects.pop_front();
                }
            }

            if (!available) {
                Debugf("Constructing new object in pool %s %u", typeid(T).name(), objectCount + 1);
                available = allocator.template allocate_object<T>();
                if (constructObject) constructObject(available);
                objectCount++;
            }
            return std::shared_ptr<T>(
                available,
                [this](T *obj) {
                    if (resetObject) resetObject(*obj);
                    std::unique_lock lock(freeMutex);
                    freeObjects.emplace_back(obj);
                },
                allocator);
        }

    private:
        std::pmr::synchronized_pool_resource memoryPool;
        std::pmr::polymorphic_allocator<T> allocator;

        LockFreeMutex freeMutex;
        std::pmr::deque<T *> freeObjects;
        std::atomic_uint32_t objectCount = 0;

        std::function<void(T *)> constructObject;
        std::function<void(T &)> resetObject;
    };
} // namespace sp
