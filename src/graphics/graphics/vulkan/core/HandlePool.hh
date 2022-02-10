#pragma once

#include "core/LockFreeMutex.hh"
#include "graphics/vulkan/core/Common.hh"

#ifdef SP_DEBUG
    #define HANDLE_POOL_DEBUG_UNFREED_HANDLES
#endif

namespace sp::vulkan {
    template<typename HandleType>
    class SharedHandle {
    public:
        struct PoolRef {
            vector<SharedHandle<HandleType>> *freeList;
            LockFreeMutex *freeMutex;

#ifdef HANDLE_POOL_DEBUG_UNFREED_HANDLES
            shared_ptr<std::atomic_flag> destroyed;
#endif
        };

        SharedHandle() {}
        SharedHandle(std::nullptr_t) {}

        SharedHandle(SharedHandle<HandleType> &&other) noexcept
            : handle(std::move(other.handle)), pool(std::move(other.pool)), useCount(other.useCount) {
            other.useCount = nullptr;
        }

        SharedHandle(const SharedHandle<HandleType> &other) noexcept
            : handle(other.handle), pool(other.pool), useCount(other.useCount) {
            if (useCount) (*useCount)++;
        }

        explicit SharedHandle(HandleType &&handle, PoolRef &&pool) : handle(std::move(handle)), pool(std::move(pool)) {
            useCount = new std::atomic_size_t(1);
        }

        ~SharedHandle() {
            Destroy();
        }

        void Reset(const SharedHandle<HandleType> &newValue) {
            Destroy();
            handle = newValue.handle;
            pool = newValue.pool;
            useCount = newValue.useCount;
            if (useCount) (*useCount)++;
        }

        SharedHandle<HandleType> &operator=(const SharedHandle<HandleType> &other) {
            Reset(other);
            return *this;
        }

        operator HandleType() {
            return handle;
        }

        operator bool() {
            return !!useCount;
        }

        HandleType &Get() {
            return handle;
        }

    private:
        void Destroy() {
            if (!useCount) return; // SharedHandle doesn't own a handle

#ifdef HANDLE_POOL_DEBUG_UNFREED_HANDLES
            Assert(!pool.destroyed->test(), "pool destroyed before this allocation");
#endif
            auto uses = useCount->fetch_sub(1);
            if (uses == 0) {
                delete useCount;
            } else if (uses == 1) {
                std::unique_lock lock(*pool.freeMutex);
                pool.freeList->push_back(std::move(*this));
            }
        }

        HandleType handle = {};
        PoolRef pool;
        std::atomic_size_t *useCount = nullptr;

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
                    (*sharedHandle.useCount)++;
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
