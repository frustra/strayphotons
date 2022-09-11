#pragma once

#include <core/Common.hh>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <graphics/vulkan/core/UniqueID.hh>
#include <string>
#include <vulkan/vulkan.hpp>

namespace sp::vulkan {
    typedef uint32 ShaderHandle;

    class DeviceContext;
    class CommandContext;
    class Buffer;
    class SubBuffer;
    class Image;
    class ImageView;
    typedef shared_ptr<CommandContext> CommandContextPtr;
    typedef shared_ptr<Buffer> BufferPtr;
    typedef shared_ptr<SubBuffer> SubBufferPtr;
    typedef shared_ptr<Image> ImagePtr;
    typedef shared_ptr<ImageView> ImageViewPtr;

    void AssertVKSuccess(vk::Result result, std::string message);
    void AssertVKSuccess(VkResult result, std::string message);

    enum QueueType { QUEUE_TYPE_GRAPHICS, QUEUE_TYPE_COMPUTE, QUEUE_TYPE_TRANSFER, QUEUE_TYPES_COUNT };

    enum class CommandContextType {
        General = QUEUE_TYPE_GRAPHICS,
        ComputeAsync = QUEUE_TYPE_COMPUTE,
        TransferAsync = QUEUE_TYPE_TRANSFER,
    };

    enum class CommandContextScope {
        Frame,
        Fence,
    };

    enum class SamplerType {
        BilinearClampEdge,
        BilinearClampBorder,
        BilinearTiled,
        TrilinearClampEdge,
        TrilinearClampBorder,
        TrilinearTiled,
        NearestClampEdge,
        NearestClampBorder,
        NearestTiled,
    };

    struct float16_t {
        uint16_t value;

        float16_t() : value(0) {}

        float16_t(const float &value_) {
            if (value_ == 0.0f) {
                value = 0u;
            } else {
                auto x = *reinterpret_cast<const uint32_t *>(&value_);
                value = ((x >> 16) & 0x8000) | ((((x & 0x7f800000) - 0x38000000) >> 13) & 0x7c00) |
                        ((x >> 13) & 0x03ff);
            }
        }

        operator float() const {
            return ((value & 0x8000) << 16) | (((value & 0x7c00) + 0x1C000) << 13) | ((value & 0x03FF) << 13);
        }
    };

    template<typename T>
    class WrappedUniqueHandle : public NonCopyable {
    public:
        T Get() const {
            return *uniqueHandle;
        }

        T operator*() const {
            return *uniqueHandle;
        }

        T *operator->() {
            T &t = *uniqueHandle;
            return &t;
        }

        operator T() const {
            return *uniqueHandle;
        }

    protected:
        vk::UniqueHandle<T, vk::DispatchLoaderDynamic> uniqueHandle;
    };
} // namespace sp::vulkan
