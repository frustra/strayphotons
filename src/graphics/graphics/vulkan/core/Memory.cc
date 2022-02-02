#include "core/Tracing.hh"
#define VMA_DEBUG_LOG(format, ...) ZoneNamed(__tracy__LINE__, true) ZoneTextV(__tracy__LINE__, format, strlen(format))

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

    void UniqueMemory::Flush() {
        // this is a no-op if memory type is HOST_COHERENT; such memory is automatically flushed
        vmaFlushAllocation(allocator, allocation, 0, VK_WHOLE_SIZE);
    }

    SubBuffer::~SubBuffer() {
        vmaVirtualFree(subAllocationBlock, offsetBytes);
    }

    void *SubBuffer::Mapped() {
        return static_cast<uint8_t *>(parentBuffer->Mapped()) + offsetBytes;
    }

    Buffer::Buffer() : UniqueMemory(VK_NULL_HANDLE) {}

    Buffer::Buffer(vk::BufferCreateInfo bufferInfo, VmaAllocationCreateInfo allocInfo, VmaAllocator allocator)
        : UniqueMemory(allocator), bufferInfo(bufferInfo) {
        ZoneScoped;

        if (bufferInfo.size == 0) return; // allow creating empty buffers for convenience

        VkBufferCreateInfo vkBufferInfo = bufferInfo;
        VkBuffer vkBuffer;

        auto result = vmaCreateBuffer(allocator, &vkBufferInfo, &allocInfo, &vkBuffer, &allocation, nullptr);
        AssertVKSuccess(result, "creating buffer");
        buffer = vkBuffer;
    }

    Buffer::~Buffer() {
        if (subAllocationBlock != VK_NULL_HANDLE) vmaDestroyVirtualBlock(subAllocationBlock);

        if (allocator != VK_NULL_HANDLE && allocation != VK_NULL_HANDLE) {
            UnmapPersistent();
            vmaDestroyBuffer(allocator, buffer, allocation);
        }
    }

    vk::DeviceSize Buffer::SubAllocateRaw(vk::DeviceSize size, vk::DeviceSize alignment) {
        if (subAllocationBlock == VK_NULL_HANDLE) {
            VmaVirtualBlockCreateInfo blockCreateInfo = {};
            blockCreateInfo.size = bufferInfo.size;
            auto result = vmaCreateVirtualBlock(&blockCreateInfo, &subAllocationBlock);
            AssertVKSuccess(result, "creating virtual block");
        }

        VmaVirtualAllocationCreateInfo allocCreateInfo = {};
        allocCreateInfo.size = size;
        allocCreateInfo.alignment = alignment;

        VkDeviceSize allocOffset;
        auto result = vmaVirtualAllocate(subAllocationBlock, &allocCreateInfo, &allocOffset);
        AssertVKSuccess(result, "creating virtual allocation");
        return allocOffset;
    }

    SubBufferPtr Buffer::ArrayAllocate(size_t elementCount, vk::DeviceSize bytesPerElement) {
        Assertf(subBufferBytesPerElement == 0 || subBufferBytesPerElement == bytesPerElement,
            "buffer is sub-allocated with %d bytes per element, not %d",
            subBufferBytesPerElement,
            bytesPerElement);

        subBufferBytesPerElement = bytesPerElement;
        auto size = elementCount * bytesPerElement;
        auto offsetBytes = SubAllocateRaw(size, 1);
        Assert(offsetBytes % bytesPerElement == 0, "suballocation was not aligned to the array");
        return make_shared<SubBuffer>(this, subAllocationBlock, offsetBytes, size, bytesPerElement);
    }

    SubBufferPtr Buffer::SubAllocate(vk::DeviceSize size, vk::DeviceSize alignment) {
        Assertf(subBufferBytesPerElement == 0 || subBufferBytesPerElement == 1,
            "buffer is sub-allocated with %d bytes per element, not 1",
            subBufferBytesPerElement);

        subBufferBytesPerElement = 1;
        auto offsetBytes = SubAllocateRaw(size, alignment);
        return make_shared<SubBuffer>(this, subAllocationBlock, offsetBytes, size);
    }
} // namespace sp::vulkan
