#pragma once

#include "graphics/vulkan/core/Common.hh"

namespace sp::vulkan {
    struct RenderTargetDesc {
        vk::Extent3D extent;
        uint32 arrayLayers = 1;
        vk::Format format = vk::Format::eUndefined;
        vk::ImageUsageFlags usage; // must include eColorAttachment or eDepthStencilAttachment to use as a render target

        bool operator==(const RenderTargetDesc &other) const {
            return extent == other.extent && arrayLayers == other.arrayLayers && format == other.format &&
                   usage == other.usage;
        }
    };

    class RenderTarget {
    public:
        RenderTarget(const RenderTargetDesc &desc, const ImageViewPtr &imageView, size_t poolIndex)
            : desc(desc), imageView(imageView), poolIndex(poolIndex) {}
        ~RenderTarget();

        const ImageViewPtr &ImageView() const {
            return imageView;
        }

    protected:
        int unusedFrames = 0;
        friend class RenderTargetManager;

    private:
        RenderTargetDesc desc;
        ImageViewPtr imageView;
        size_t poolIndex;
    };

    class RenderTargetManager {
    public:
        RenderTargetManager(DeviceContext &device) : device(device) {}
        RenderTargetPtr Get(const RenderTargetDesc &desc);
        void TickFrame();

    private:
        DeviceContext &device;
        vector<RenderTargetPtr> pool;
    };
} // namespace sp::vulkan
