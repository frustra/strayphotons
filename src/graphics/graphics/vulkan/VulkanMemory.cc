#include "VulkanMemory.hh"

#include "core/Common.hh"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

namespace sp {
    VulkanUniqueBuffer::VulkanUniqueBuffer() : VulkanUniqueMemory(VK_NULL_HANDLE) {
        Release();
    }

    VulkanUniqueBuffer::VulkanUniqueBuffer(vk::BufferCreateInfo bufferInfo,
                                           VmaAllocationCreateInfo allocInfo,
                                           VmaAllocator allocator)
        : VulkanUniqueMemory(allocator), bufferInfo(bufferInfo) {

        VkBufferCreateInfo vkBufferInfo = bufferInfo;
        VkBuffer vkBuffer;

        auto result = vmaCreateBuffer(allocator, &vkBufferInfo, &allocInfo, &vkBuffer, &allocation, nullptr);
        AssertVKSuccess(result, "creating command buffer");
        buffer = vkBuffer;
    }

    VulkanUniqueBuffer::~VulkanUniqueBuffer() {
        Destroy();
    }

    VulkanUniqueBuffer &VulkanUniqueBuffer::operator=(VulkanUniqueBuffer &&other) {
        Destroy();
        allocator = other.allocator;
        allocation = other.allocation;
        buffer = other.buffer;
        bufferInfo = other.bufferInfo;
        other.Release();
        return *this;
    }

    void VulkanUniqueBuffer::Destroy() {
        if (allocator != VK_NULL_HANDLE) {
            vmaDestroyBuffer(allocator, buffer, allocation);
            Release();
        }
    }

    void VulkanUniqueBuffer::Release() {
        allocator = VK_NULL_HANDLE;
        allocation = VK_NULL_HANDLE;
        buffer = VK_NULL_HANDLE;
        bufferInfo = vk::BufferCreateInfo();
    }

    vk::Result VulkanUniqueMemory::Map(void **data) {
        return static_cast<vk::Result>(vmaMapMemory(allocator, allocation, data));
    }

    void VulkanUniqueMemory::Unmap() {
        vmaUnmapMemory(allocator, allocation);
    }
} // namespace sp
