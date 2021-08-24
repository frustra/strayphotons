#define VMA_IMPLEMENTATION
#include "Memory.hh"

#include "core/Common.hh"

namespace sp::vulkan {
    UniqueBuffer::UniqueBuffer() : UniqueMemory(VK_NULL_HANDLE) {
        Release();
    }

    UniqueBuffer::UniqueBuffer(vk::BufferCreateInfo bufferInfo,
                               VmaAllocationCreateInfo allocInfo,
                               VmaAllocator allocator)
        : UniqueMemory(allocator), bufferInfo(bufferInfo) {

        VkBufferCreateInfo vkBufferInfo = bufferInfo;
        VkBuffer vkBuffer;

        auto result = vmaCreateBuffer(allocator, &vkBufferInfo, &allocInfo, &vkBuffer, &allocation, nullptr);
        AssertVKSuccess(result, "creating command buffer");
        buffer = vkBuffer;
    }

    UniqueBuffer::~UniqueBuffer() {
        Destroy();
    }

    UniqueBuffer &UniqueBuffer::operator=(UniqueBuffer &&other) {
        Destroy();
        allocator = other.allocator;
        allocation = other.allocation;
        buffer = other.buffer;
        bufferInfo = other.bufferInfo;
        other.Release();
        return *this;
    }

    void UniqueBuffer::Destroy() {
        if (allocator != VK_NULL_HANDLE) {
            vmaDestroyBuffer(allocator, buffer, allocation);
            Release();
        }
    }

    void UniqueBuffer::Release() {
        allocator = VK_NULL_HANDLE;
        allocation = VK_NULL_HANDLE;
        buffer = VK_NULL_HANDLE;
        bufferInfo = vk::BufferCreateInfo();
    }

    vk::Result UniqueMemory::Map(void **data) {
        return static_cast<vk::Result>(vmaMapMemory(allocator, allocation, data));
    }

    void UniqueMemory::Unmap() {
        vmaUnmapMemory(allocator, allocation);
    }
} // namespace sp::vulkan
