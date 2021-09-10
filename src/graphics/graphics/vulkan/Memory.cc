#define VMA_IMPLEMENTATION
#include "Memory.hh"

#include "core/Common.hh"

namespace sp::vulkan {
    vk::Result UniqueMemory::Map(void **data) {
        return static_cast<vk::Result>(vmaMapMemory(allocator, allocation, data));
    }

    void UniqueMemory::Unmap() {
        vmaUnmapMemory(allocator, allocation);
    }

    vk::DeviceSize UniqueMemory::ByteSize() const {
        return allocation ? allocation->GetSize() : 0;
    }

    Buffer::Buffer() : UniqueMemory(VK_NULL_HANDLE) {}

    Buffer::Buffer(vk::BufferCreateInfo bufferInfo, VmaAllocationCreateInfo allocInfo, VmaAllocator allocator)
        : UniqueMemory(allocator), bufferInfo(bufferInfo) {

        if (bufferInfo.size == 0) return; // allow creating empty buffers for convenience

        VkBufferCreateInfo vkBufferInfo = bufferInfo;
        VkBuffer vkBuffer;

        auto result = vmaCreateBuffer(allocator, &vkBufferInfo, &allocInfo, &vkBuffer, &allocation, nullptr);
        AssertVKSuccess(result, "creating buffer");
        buffer = vkBuffer;
    }

    Buffer::~Buffer() {
        if (allocator != VK_NULL_HANDLE && allocation != VK_NULL_HANDLE) {
            vmaDestroyBuffer(allocator, buffer, allocation);
        }
    }
} // namespace sp::vulkan
