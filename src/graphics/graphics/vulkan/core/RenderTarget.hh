#pragma once

#include "core/Hashing.hh"
#include "graphics/vulkan/core/Common.hh"

namespace sp::vulkan {
    struct RenderTargetDesc {
        vk::Extent3D extent;
        uint32 mipLevels = 1;
        uint32 arrayLayers = 1;
        vk::Format format = vk::Format::eUndefined;
        vk::ImageUsageFlags usage; // must include eColorAttachment or eDepthStencilAttachment to use as a render target
        vk::ImageType imageType = vk::ImageType::e2D;
        vk::ImageViewType primaryViewType = vk::ImageViewType::e2D; // when e2D, derived from imageType
        SamplerType sampler = SamplerType::BilinearClamp;

        bool operator==(const RenderTargetDesc &other) const = default;

        vk::ImageViewType DeriveViewType() const {
            switch (imageType) {
            case vk::ImageType::e1D:
                return vk::ImageViewType::e1D;
            case vk::ImageType::e2D:
                return vk::ImageViewType::e2D;
            case vk::ImageType::e3D:
                return vk::ImageViewType::e3D;
            }
            Abort("invalid vk::ImageType");
        }
    };

    class RenderTarget {
    public:
        RenderTarget(DeviceContext &device, const RenderTargetDesc &desc, const ImageViewPtr &imageView)
            : device(&device), desc(desc), imageView(imageView) {}

        const ImageViewPtr &ImageView() const {
            return imageView;
        }

        const ImageViewPtr &LayerImageView(uint32 layer);

        const RenderTargetDesc &Desc() const {
            return desc;
        }

        int unusedFrames = 0;

    private:
        DeviceContext *device;
        RenderTargetDesc desc;
        ImageViewPtr imageView;
        vector<ImageViewPtr> layerImageViews;
    };
} // namespace sp::vulkan
