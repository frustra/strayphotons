#pragma once

#include "Common.hh"

#include <vk_mem_alloc.h>

namespace sp::vulkan {
    struct UniqueMemory {
        UniqueMemory() = delete;
        UniqueMemory(VmaAllocator allocator) : allocator(allocator) {}
        vk::Result Map(void **data);
        void Unmap();

    protected:
        VmaAllocator allocator;
        VmaAllocation allocation;
    };

    struct UniqueBuffer : public UniqueMemory {
        UniqueBuffer();
        UniqueBuffer(vk::BufferCreateInfo bufferInfo, VmaAllocationCreateInfo allocInfo, VmaAllocator allocator);
        ~UniqueBuffer();
        void Destroy();

        UniqueBuffer &operator=(UniqueBuffer const &) = delete;
        UniqueBuffer &operator=(UniqueBuffer &&other);

        vk::Buffer Get() {
            return buffer;
        }

        vk::DeviceSize Size() {
            return bufferInfo.size;
        }

    protected:
        void Release();

        vk::BufferCreateInfo bufferInfo;
        vk::Buffer buffer;
    };
} // namespace sp::vulkan
