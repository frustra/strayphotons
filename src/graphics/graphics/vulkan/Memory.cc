#define VMA_IMPLEMENTATION
#include "Memory.hh"

#include "core/Common.hh"

namespace sp::vulkan {
    vk::Result UniqueMemory::MapPersistent(void **data) {
        auto result = vk::Result::eSuccess;
        if (!persistentMap) result = Map(&persistentMap);
        if (data) *data = persistentMap;
        return result;
    }

    vk::Result UniqueMemory::Map(void **data) {
        if (persistentMap) {
            *data = persistentMap;
            return vk::Result::eSuccess;
        }
        return static_cast<vk::Result>(vmaMapMemory(allocator, allocation, data));
    }

    void UniqueMemory::Unmap() {
        if (!persistentMap) vmaUnmapMemory(allocator, allocation);
    }

    void UniqueMemory::UnmapPersistent() {
        if (persistentMap) {
            vmaUnmapMemory(allocator, allocation);
            persistentMap = nullptr;
        }
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
            UnmapPersistent();
            vmaDestroyBuffer(allocator, buffer, allocation);
        }
    }
} // namespace sp::vulkan
