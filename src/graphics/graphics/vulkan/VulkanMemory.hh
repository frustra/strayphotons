#pragma once

#include "Common.hh"

#include <vk_mem_alloc.h>

namespace sp {
    struct VulkanUniqueMemory {
        VulkanUniqueMemory() = delete;
        VulkanUniqueMemory(VmaAllocator allocator) : allocator(allocator) {}
        vk::Result Map(void **data);
        void Unmap();

    protected:
        VmaAllocator allocator;
        VmaAllocation allocation;
    };

    struct VulkanUniqueBuffer : public VulkanUniqueMemory {
        VulkanUniqueBuffer();
        VulkanUniqueBuffer(vk::BufferCreateInfo bufferInfo, VmaAllocationCreateInfo allocInfo, VmaAllocator allocator);
        ~VulkanUniqueBuffer();
        void Destroy();

        VulkanUniqueBuffer &operator=(VulkanUniqueBuffer const &) = delete;
        VulkanUniqueBuffer &operator=(VulkanUniqueBuffer &&other);

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
} // namespace sp
