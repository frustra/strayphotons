#pragma once

#include <core/Common.hh>
#include <graphics/vulkan/core/UniqueID.hh>
#include <string>
#include <vulkan/vulkan.hpp>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace sp::vulkan {
    typedef uint32 ShaderHandle;
    typedef uint32 RenderGraphResourceID;

    class DeviceContext;
    class CommandContext;
    class Buffer;
    class SubBuffer;
    class Image;
    class ImageView;
    class RenderTarget;
    typedef shared_ptr<CommandContext> CommandContextPtr;
    typedef shared_ptr<Buffer> BufferPtr;
    typedef shared_ptr<SubBuffer> SubBufferPtr;
    typedef shared_ptr<Image> ImagePtr;
    typedef shared_ptr<ImageView> ImageViewPtr;
    typedef shared_ptr<RenderTarget> RenderTargetPtr;

    void AssertVKSuccess(vk::Result result, std::string message);
    void AssertVKSuccess(VkResult result, std::string message);

    enum QueueType { QUEUE_TYPE_GRAPHICS, QUEUE_TYPE_COMPUTE, QUEUE_TYPE_TRANSFER, QUEUE_TYPES_COUNT };

    enum class CommandContextType {
        General = QUEUE_TYPE_GRAPHICS,
        ComputeAsync = QUEUE_TYPE_COMPUTE,
        TransferAsync = QUEUE_TYPE_TRANSFER,
    };

    enum class SamplerType {
        BilinearClamp,
        BilinearTiled,
        TrilinearClamp,
        TrilinearTiled,
        NearestClamp,
        NearestTiled,
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
