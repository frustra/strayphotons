#pragma once

#include "Common.hh"
#include "core/Common.hh"

#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wunused-variable"
#endif

#ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wunused-variable"
#endif

#include <vk_mem_alloc.h>

#ifdef __GNUC__
    #pragma GCC diagnostic pop
#endif

#ifdef __clang__
    #pragma clang diagnostic pop
#endif

namespace sp::vulkan {
    struct UniqueMemory : public NonCopyable {
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
        UniqueBuffer(UniqueBuffer &&other);
        ~UniqueBuffer();
        void Destroy();

        UniqueBuffer &operator=(UniqueBuffer &&other);

        vk::Buffer operator*() {
            return buffer;
        }

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

    struct UniqueImage : public UniqueMemory {
        UniqueImage();
        UniqueImage(vk::ImageCreateInfo imageInfo, VmaAllocationCreateInfo allocInfo, VmaAllocator allocator);
        UniqueImage(UniqueImage &&other);
        ~UniqueImage();
        void Destroy();

        UniqueImage &operator=(UniqueImage &&other);

        vk::Image operator*() {
            return image;
        }

        vk::Image Get() {
            return image;
        }

    protected:
        void Release();

        vk::ImageCreateInfo imageInfo;
        vk::Image image;
    };
} // namespace sp::vulkan
