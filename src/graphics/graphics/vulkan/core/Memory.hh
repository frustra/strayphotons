/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "core/Common.hh"
#include "core/Hashing.hh"
#include "core/Tracing.hh"
#include "graphics/vulkan/core/Access.hh"
#include "graphics/vulkan/core/VkCommon.hh"

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4127 4189 4324)
#endif

#define VMA_DEBUG_LOG(...) Tracef(__VA_ARGS__)

#include <vk_mem_alloc.h>

#ifdef _MSC_VER
    #pragma warning(pop)
#endif

namespace sp::vulkan {
    enum class Residency {
        UNKNOWN = VMA_MEMORY_USAGE_UNKNOWN,
        CPU_ONLY = VMA_MEMORY_USAGE_CPU_ONLY,
        CPU_TO_GPU = VMA_MEMORY_USAGE_CPU_TO_GPU,
        GPU_ONLY = VMA_MEMORY_USAGE_GPU_ONLY,
        GPU_TO_CPU = VMA_MEMORY_USAGE_GPU_TO_CPU,
    };

    struct BufferLayout {
        BufferLayout() = default;
        BufferLayout(size_t size) : size(size) {}
        BufferLayout(size_t arrayStride, size_t arrayCount)
            : size(arrayStride * arrayCount), arrayStride(arrayStride), arrayCount(arrayCount) {}
        BufferLayout(size_t baseSize, size_t arrayStride, size_t arrayCount)
            : size(baseSize + arrayStride * arrayCount), arrayStride(arrayStride), arrayCount(arrayCount) {}

        size_t size = 0, arrayStride = 0, arrayCount = 0;

        bool operator==(const BufferLayout &) const = default;
    };

    struct BufferDesc {
        BufferLayout layout;
        vk::BufferUsageFlags usage;
        Residency residency = Residency::UNKNOWN;

        bool operator==(const BufferDesc &) const = default;
    };

    struct InitialData {
        const uint8 *data = nullptr;
        size_t dataSize = 0;
        shared_ptr<const void> dataOwner;

        InitialData() = default;
        InitialData(const uint8 *data, size_t dataSize, const shared_ptr<const void> &dataOwner = nullptr)
            : data(data), dataSize(dataSize), dataOwner(dataOwner) {}
    };

    class UniqueMemory : public NonCopyable, public HasUniqueID {
    public:
        UniqueMemory() = delete;
        UniqueMemory(VmaAllocator allocator) : allocator(allocator), allocation(nullptr) {}
        virtual ~UniqueMemory() = default;

        void *Mapped() {
            void *data;
            AssertVKSuccess(MapPersistent(&data), "MapPersistent failed");
            return data;
        }

        vk::Result MapPersistent(void **data = nullptr);
        vk::Result Map(void **data);
        void Unmap();
        void UnmapPersistent();
        vk::DeviceSize ByteSize() const;
        vk::MemoryPropertyFlags Properties() const;

        const size_t CopyBlockSize = 512 * 1024;

        template<typename T>
        void CopyFrom(const T *srcData, size_t srcCount = 1, size_t dstOffset = 0) {
            if (srcCount == 0) return;

            ZoneScoped;
            Assert(sizeof(T) * (dstOffset + srcCount) <= ByteSize(), "UniqueMemory overflow");
            T *dstData;
            Map((void **)&dstData);
            {
                ZoneScopedN("memcpy");
                ZoneValue(srcCount * sizeof(T));
                auto srcEnd = srcData + srcCount;
                auto window = std::chrono::milliseconds(1);
                for (size_t offset = 0; offset < srcCount; offset += CopyBlockSize) {
                    auto srcBlock = srcData + offset;

                    ClockTimer timer;
                    std::copy(srcBlock, std::min(srcEnd, srcBlock + CopyBlockSize), dstData + dstOffset + offset);

                    if (offset + CopyBlockSize < srcCount) {
                        auto elapsed = timer.Duration();
                        if (elapsed > window) {
                            ZoneScopedN("sleep");
                            ZoneValue(elapsed.count());
                            std::this_thread::sleep_for(elapsed);
                        }
                    }
                }
            }
            Unmap();
            Flush();
        }

        void Flush();

    protected:
        VmaAllocator allocator;
        VmaAllocation allocation;
        void *persistentMap = nullptr;
    };

    class SubBuffer : public NonCopyable, public HasUniqueID {
    public:
        SubBuffer(Buffer *buffer,
            VmaVirtualBlock subAllocationBlock,
            vk::DeviceSize offsetBytes,
            vk::DeviceSize size,
            size_t arrayOffset = std::numeric_limits<vk::DeviceSize>::max(),
            size_t arrayCount = 0)
            : parentBuffer(buffer), subAllocationBlock(subAllocationBlock), offsetBytes(offsetBytes), size(size),
              arrayOffset(arrayOffset), arrayCount(arrayCount) {}

        ~SubBuffer();

        void *Mapped();

        size_t ArrayOffset() const {
            return arrayOffset;
        }

        size_t ArrayCount() const {
            return arrayCount;
        }

        vk::DeviceSize ByteSize() const {
            return size;
        }

        vk::DeviceSize ByteOffset() const {
            return offsetBytes;
        }

        operator vk::Buffer() const;

    private:
        Buffer *parentBuffer;
        VmaVirtualBlock subAllocationBlock;
        vk::DeviceSize offsetBytes, size;
        size_t arrayOffset, arrayCount;
    };

    class Buffer : public UniqueMemory {
    public:
        Buffer();
        Buffer(vk::BufferCreateInfo bufferInfo,
            VmaAllocationCreateInfo allocInfo,
            VmaAllocator allocator,
            size_t arrayStride = 0,
            size_t arrayCount = 0);
        ~Buffer();

        vk::Buffer operator*() const {
            return buffer;
        }

        operator vk::Buffer() const {
            return buffer;
        }

        vk::DeviceSize ByteSize() const {
            return bufferInfo.size;
        }

        size_t ArraySize() const {
            return arrayCount;
        }

        vk::BufferUsageFlags Usage() const {
            return bufferInfo.usage;
        }

        /**
         * Treats the buffer as an array with all elements having `arrayStride` size.
         * Allocates `elementCount` elements from this array.
         * Can be called only if the buffer was created with an arrayStride.
         *
         * If bufferInfo.size is not a multiple of arrayStride, the array is aligned to the end of the buffer,
         * since runtime sized arrays are placed at the end of GPU buffers in shader storage.
         */
        SubBufferPtr ArrayAllocate(size_t elementCount);

        vk::DeviceSize ArrayStride() const {
            return arrayStride;
        }

        /**
         * Treats the buffer as a heap containing arbitrarily sized allocations.
         * Can be called only if the buffer was created without an arrayStride.
         */
        SubBufferPtr SubAllocate(vk::DeviceSize size, vk::DeviceSize alignment = 4);

        Access LastAccess() const {
            return lastAccess;
        }

        void SetAccess(Access oldAccess, Access newAccess);

    private:
        vk::DeviceSize SubAllocateRaw(vk::DeviceSize size, vk::DeviceSize alignment);

        vk::BufferCreateInfo bufferInfo;
        vk::Buffer buffer;

        size_t arrayStride = 0, arrayCount = 0;
        VmaVirtualBlock subAllocationBlock = VK_NULL_HANDLE;

        Access lastAccess = Access::None;
    };
} // namespace sp::vulkan

namespace std {
    template<>
    struct hash<sp::vulkan::BufferDesc> {
        typedef sp::vulkan::BufferDesc argument_type;
        typedef std::size_t result_type;

        result_type operator()(argument_type const &s) const {
            result_type h = 0;
            sp::hash_combine(h, s.layout.size);
            sp::hash_combine(h, s.layout.arrayStride);
            sp::hash_combine(h, s.layout.arrayCount);
            sp::hash_combine(h, VkBufferUsageFlags(s.usage));
            sp::hash_combine(h, s.residency);
            return h;
        }
    };
} // namespace std
