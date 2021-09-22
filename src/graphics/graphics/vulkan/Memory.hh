#pragma once

#include "Common.hh"
#include "core/Common.hh"

#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wunused-variable"
    #pragma clang diagnostic ignored "-Wnullability-completeness"
#endif

#ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wunused-variable"
#endif

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4127 4324)
#endif

#include <vk_mem_alloc.h>

#ifdef __GNUC__
    #pragma GCC diagnostic pop
#endif

#ifdef __clang__
    #pragma clang diagnostic pop
#endif

#ifdef _MSC_VER
    #pragma warning(pop)
#endif

namespace sp::vulkan {
    enum BufferType { BUFFER_TYPE_UNIFORM, BUFFER_TYPES_COUNT };

    class UniqueMemory : public NonCopyable {
    public:
        UniqueMemory() = delete;
        UniqueMemory(VmaAllocator allocator) : allocator(allocator), allocation(nullptr) {}
        virtual ~UniqueMemory() = default;

        vk::Result MapPersistent(void **data = nullptr);
        vk::Result Map(void **data);
        void Unmap();
        void UnmapPersistent();
        vk::DeviceSize ByteSize() const;

        template<typename T>
        void CopyFrom(const T *srcData, size_t srcCount = 1, size_t dstOffset = 0) {
            Assert(sizeof(T) * (dstOffset + srcCount) <= ByteSize(), "UniqueMemory overflow");
            T *dstData;
            Map((void **)&dstData);
            std::copy(srcData, srcData + srcCount, dstData + dstOffset);
            Unmap();
        }

    protected:
        VmaAllocator allocator;
        VmaAllocation allocation;
        void *persistentMap = nullptr;
    };

    class Buffer : public UniqueMemory {
    public:
        Buffer();
        Buffer(vk::BufferCreateInfo bufferInfo, VmaAllocationCreateInfo allocInfo, VmaAllocator allocator);
        ~Buffer();

        vk::Buffer operator*() const {
            return buffer;
        }

        operator vk::Buffer() const {
            return buffer;
        }

        vk::DeviceSize Size() const {
            return bufferInfo.size;
        }

    private:
        vk::BufferCreateInfo bufferInfo;
        vk::Buffer buffer;
    };
} // namespace sp::vulkan
