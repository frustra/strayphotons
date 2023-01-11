#pragma once

#include "core/Hashing.hh"
#include "graphics/vulkan/core/Common.hh"

namespace sp::vulkan::render_graph {
    struct ImageDesc {
        vk::Extent3D extent;
        uint32 mipLevels = 1;
        uint32 arrayLayers = 1;
        vk::Format format = vk::Format::eUndefined;
        vk::ImageType imageType = vk::ImageType::e2D;
        vk::ImageViewType primaryViewType = vk::ImageViewType::e2D; // when e2D, derived from imageType
        SamplerType sampler = SamplerType::BilinearClampEdge;

        vk::ImageUsageFlags usage; // set by the render graph

        bool operator==(const ImageDesc &) const = default;

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

    class PooledImage {
    public:
        PooledImage(DeviceContext &device, const ImageDesc &desc, const ImageViewPtr &imageView)
            : device(&device), desc(desc), imageView(imageView) {}

        const ImageViewPtr &ImageView() const {
            return imageView;
        }

        const ImageViewPtr &LayerImageView(uint32 layer);
        const ImageViewPtr &MipImageView(uint32 mip);
        const ImageViewPtr &DepthImageView();

        const ImageDesc &Desc() const {
            return desc;
        }

    protected:
        friend class Resources;
        int unusedFrames = 0;

    private:
        DeviceContext *device;
        ImageDesc desc;
        ImageViewPtr imageView;
        vector<ImageViewPtr> layerImageViews;
        vector<ImageViewPtr> mipImageViews;
        ImageViewPtr depthImageView;
    };

    typedef shared_ptr<PooledImage> PooledImagePtr;
} // namespace sp::vulkan::render_graph
