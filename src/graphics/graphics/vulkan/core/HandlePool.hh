#pragma once

#include "graphics/vulkan/core/Common.hh"

namespace sp::vulkan {
    template<typename HandleType>
    class SharedHandle {
    public:
        SharedHandle() {}
        SharedHandle(std::nullptr_t) {}

        SharedHandle(HandleType handle, vector<SharedHandle<HandleType>> &freeList)
            : handle(handle), freeList(&freeList) {
            ptr = make_shared<bool>(true);
        }

        ~SharedHandle() {
            if (ptr && ptr.use_count() == 1) { freeList->push_back(*this); }
        }

        operator HandleType() {
            return handle;
        }

        operator bool() {
            return !!ptr;
        }

        HandleType &Get() {
            return handle;
        }

    private:
        HandleType handle = {};
        vector<SharedHandle<HandleType>> *freeList = nullptr;
        shared_ptr<bool> ptr = {}; // used only for refcounting this object

        template<typename>
        friend class HandlePool;
    };

    template<typename HandleType>
    class HandlePool {
    public:
        HandlePool(std::function<HandleType()> createObject,
            std::function<void(HandleType)> destroyObject,
            std::function<void(HandleType)> resetObject = {})
            : createObject(createObject), destroyObject(destroyObject), resetObject(resetObject) {}

        HandlePool() = default;

        ~HandlePool() {
            Assert(freeObjects.size() == totalObjects, "some objects weren't freed before the pool");
            for (auto &object : freeObjects) {
                destroyObject(object);
                object.ptr.reset();
            }
        }

        SharedHandle<HandleType> Get() {
            if (!freeObjects.empty()) {
                SharedHandle<HandleType> sharedHandle = freeObjects.back();
                freeObjects.pop_back();
                if (resetObject) resetObject(sharedHandle);
                return sharedHandle;
            }

            HandleType object = createObject();
            totalObjects++;
            return {object, freeObjects};
        }

    private:
        std::function<HandleType()> createObject;
        std::function<void(HandleType)> destroyObject, resetObject;
        vector<SharedHandle<HandleType>> freeObjects;
        size_t totalObjects = 0;
    };
} // namespace sp::vulkan
