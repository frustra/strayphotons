/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#define VMA_IMPLEMENTATION
#include "Memory.hh"

#include "common/Common.hh"

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

    vk::MemoryPropertyFlags UniqueMemory::Properties() const {
        auto flags = allocator->m_MemProps.memoryTypes[allocation->GetMemoryTypeIndex()].propertyFlags;
        return vk::MemoryPropertyFlags(flags);
    }

    void UniqueMemory::Flush() {
        // Flush is a no-op if memory type is HOST_COHERENT; such memory is automatically flushed
        if (allocator->IsMemoryTypeNonCoherent(allocation->GetMemoryTypeIndex())) {
            vmaFlushAllocation(allocator, allocation, 0, VK_WHOLE_SIZE);
        }
    }

    SubBuffer::~SubBuffer() {
        vmaVirtualFree(subAllocationBlock, offsetBytes);
    }

    void *SubBuffer::Mapped() {
        return static_cast<uint8_t *>(parentBuffer->Mapped()) + offsetBytes;
    }

    SubBuffer::operator vk::Buffer() const {
        return (vk::Buffer)*parentBuffer;
    }

    Buffer::Buffer() : UniqueMemory(VK_NULL_HANDLE) {}

    Buffer::Buffer(vk::BufferCreateInfo bufferInfo,
        VmaAllocationCreateInfo allocInfo,
        VmaAllocator allocator,
        size_t arrayStride,
        size_t arrayCount)
        : UniqueMemory(allocator), bufferInfo(bufferInfo), arrayStride(arrayStride), arrayCount(arrayCount) {
        ZoneScoped;
        ZoneValue(bufferInfo.size);

        if (bufferInfo.size == 0) return; // allow creating empty buffers for convenience

        VkBufferCreateInfo vkBufferInfo = (const VkBufferCreateInfo &)bufferInfo;
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

    static char *virtualAllocInfoString(VmaVirtualBlock block) {
        // Leaks str, use only before aborting.
        char *str;
        vmaBuildVirtualBlockStatsString(block, &str, false);
        return str;
    }

    vk::DeviceSize Buffer::SubAllocateRaw(vk::DeviceSize size, vk::DeviceSize alignment) {
        if (subAllocationBlock == VK_NULL_HANDLE) {
            VmaVirtualBlockCreateInfo blockCreateInfo = {};
            if (arrayStride > 0 && arrayCount > 0) {
                blockCreateInfo.size = arrayStride * arrayCount;
                DebugAssert(blockCreateInfo.size <= bufferInfo.size,
                    "declared array size greater than actual buffer size");
            } else {
                blockCreateInfo.size = bufferInfo.size;
            }
            auto result = vmaCreateVirtualBlock(&blockCreateInfo, &subAllocationBlock);
            AssertVKSuccess(result, "creating virtual block");
        }

        VmaVirtualAllocationCreateInfo allocCreateInfo = {};
        allocCreateInfo.size = size;
        allocCreateInfo.alignment = alignment;

        vk::DeviceSize allocOffset;
        auto result = vmaVirtualAllocate(subAllocationBlock, &allocCreateInfo, &allocOffset);
        Assertf(result != VK_ERROR_OUT_OF_DEVICE_MEMORY,
            "out of memory in buffer, trying to suballocate %d bytes, buffer size %d bytes\n%s",
            size,
            bufferInfo.size,
            virtualAllocInfoString(subAllocationBlock));
        AssertVKSuccess(result, "creating virtual allocation");
        return allocOffset;
    }

    SubBufferPtr Buffer::ArrayAllocate(size_t elementCount) {
        Assertf(arrayStride > 0, "buffer is not an array");

        auto size = elementCount * arrayStride;
        auto offsetBytes = SubAllocateRaw(size, 1);
        DebugAssert(offsetBytes % arrayStride == 0, "suballocation was not aligned to the array");
        offsetBytes += bufferInfo.size - arrayCount * arrayStride; // align to end
        return make_shared<SubBuffer>(this,
            subAllocationBlock,
            offsetBytes,
            size,
            offsetBytes / arrayStride,
            elementCount);
    }

    SubBufferPtr Buffer::SubAllocate(vk::DeviceSize size, vk::DeviceSize alignment) {
        Assertf(arrayStride == 0, "buffer is an array");

        auto offsetBytes = SubAllocateRaw(size, alignment);
        return make_shared<SubBuffer>(this, subAllocationBlock, offsetBytes, size);
    }

    void Buffer::SetAccess(Access oldAccess, Access newAccess) {
        DebugAssert(oldAccess == Access::None || oldAccess == lastAccess, "unexpected access");
        lastAccess = newAccess;
    }
} // namespace sp::vulkan
