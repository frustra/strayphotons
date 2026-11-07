/*
 * Stray Photons - Copyright (C) 2026 Jacob Wirth & Justine Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "strayphotons/Logging.hh"
#define VMA_IMPLEMENTATION
#include "Memory.hh"
#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/core/VkCommon.hh"

#include <memory>
#include <string>

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

    SubBuffer::SubBuffer() {}

    SubBuffer::~SubBuffer() {
        Assertf(handle.use_count() == 0, "SubBuffer destroyed while referenced");
        vmaVirtualFree(subAllocationBlock, offsetBytes);
    }

    SubBufferPtr SubBuffer::New(BufferPtr buffer,
        VmaVirtualBlock subAllocationBlock,
        vk::DeviceSize offsetBytes,
        vk::DeviceSize size,
        size_t arrayOffset,
        size_t arrayCount) {
        Assertf(buffer, "SubBuffer created from null parent bufffer");
        auto handle = std::make_shared<SubBuffer>();
        handle->handle = handle;
        handle->parentBuffer = buffer;
        handle->subAllocationBlock = subAllocationBlock;
        handle->offsetBytes = offsetBytes;
        handle->size = size;
        handle->arrayOffset = arrayOffset;
        handle->arrayCount = arrayCount;
        return handle;
    }

    void *SubBuffer::Mapped() {
        return static_cast<uint8_t *>(parentBuffer->Mapped()) + offsetBytes;
    }

    SubBuffer::operator vk::Buffer() const {
        Assertf(parentBuffer, "SubBuffer dereferencing null parent buffer");
        return (vk::Buffer)*parentBuffer;
    }

    void SubBuffer::AcquireAccess(CommandContext &cmd, Access newAccess) {
        Assertf(parentBuffer, "SubBuffer::AcquireAccess called no null buffer");
        parentBuffer->AcquireAccess(cmd, newAccess);
    }

    Buffer::Buffer() : UniqueMemory(VK_NULL_HANDLE) {}

    Buffer::~Buffer() {
        Assertf(handle.use_count() == 0, "Buffer destroyed while referenced");
        if (subAllocationBlock != VK_NULL_HANDLE) vmaDestroyVirtualBlock(subAllocationBlock);

        if (allocator != VK_NULL_HANDLE && allocation != VK_NULL_HANDLE) {
            UnmapPersistent();
            vmaDestroyBuffer(allocator, buffer, allocation);
        }
    }

    BufferPtr Buffer::New(vk::BufferCreateInfo bufferInfo,
        VmaAllocationCreateInfo allocInfo,
        VmaAllocator allocator,
        size_t arrayStride,
        size_t arrayCount) {
        ZoneScoped;
        ZoneValue(bufferInfo.size);
        auto handle = std::make_shared<Buffer>();
        handle->handle = handle;
        handle->allocator = allocator;
        handle->bufferInfo = bufferInfo;
        handle->arrayStride = arrayStride;
        handle->arrayCount = arrayCount;

        // allow creating empty buffers for convenience
        if (bufferInfo.size > 0) {
            VkBuffer vkBuffer;
            auto result = vmaCreateBuffer(allocator,
                &static_cast<const VkBufferCreateInfo &>(bufferInfo),
                &allocInfo,
                &vkBuffer,
                &handle->allocation,
                nullptr);
            AssertVKSuccess(result, "creating buffer");
            handle->buffer = vkBuffer;
        }
        return handle;
    }

    void Buffer::AcquireAccess(CommandContext &cmd, Access newAccess) {
        auto bufferPtr = handle.lock();
        Assertf(bufferPtr, "Buffer::AcquireAccess called on buffer with invalid handle");

        DeviceContext &device = cmd.Device();
        uint32_t queueFamilyIndex = device.QueueFamilyIndex(cmd.GetType());

        if (lastQueueFamilyIndex != queueFamilyIndex) {
            DebugZoneScopedN("AcquireBuffer");
            if (lastQueueFamilyIndex != VK_QUEUE_FAMILY_IGNORED) {
                ZoneScopedN("TransferQueueFamily");
                auto subCmd = device.GetFencedCommandContext(device.GetQueueType(lastQueueFamilyIndex));

                BufferBarrierInfo options = {};
                options.srcQueueFamilyIndex = lastQueueFamilyIndex;
                options.dstQueueFamilyIndex = queueFamilyIndex;
                const AccessInfo &lastInfo = GetAccessInfo(lastAccess);
                subCmd->BufferBarrier(bufferPtr, lastInfo.stageMask, lastInfo.accessMask, options);
                subCmd->acquiredBuffers.emplace_back(bufferPtr);

                device.Submit(subCmd);
            }

            // Acquire new queue family
            BufferBarrierInfo options = {};
            options.srcQueueFamilyIndex = lastQueueFamilyIndex;
            options.dstQueueFamilyIndex = queueFamilyIndex;
            const AccessInfo &accessInfo = GetAccessInfo(newAccess);
            cmd.BufferBarrier(bufferPtr, accessInfo.stageMask, accessInfo.accessMask, options);
            lastQueueFamilyIndex = queueFamilyIndex;
            lastAccess = newAccess;
        } else {
            // No queue ownership transfer required
            const AccessInfo &accessInfo = GetAccessInfo(newAccess);
            cmd.BufferBarrier(bufferPtr, accessInfo.stageMask, accessInfo.accessMask);
            lastAccess = newAccess;
        }
        cmd.acquiredBuffers.emplace_back(bufferPtr);
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
        return SubBuffer::New(handle.lock(),
            subAllocationBlock,
            offsetBytes,
            size,
            offsetBytes / arrayStride,
            elementCount);
    }

    SubBufferPtr Buffer::SubAllocate(vk::DeviceSize size, vk::DeviceSize alignment) {
        Assertf(arrayStride == 0, "buffer is an array");

        auto offsetBytes = SubAllocateRaw(size, alignment);
        return SubBuffer::New(handle.lock(), subAllocationBlock, offsetBytes, size);
    }

    void Buffer::SetAccess(Access oldAccess, Access newAccess) {
        DebugAssert(oldAccess == Access::None || oldAccess == lastAccess, "unexpected access");
        lastAccess = newAccess;
    }
} // namespace sp::vulkan
