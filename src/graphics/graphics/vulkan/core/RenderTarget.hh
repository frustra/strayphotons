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
        RenderTarget(DeviceContext &device,
            const RenderTargetDesc &desc,
            const ImageViewPtr &imageView,
            size_t poolIndex)
            : device(&device), desc(desc), imageView(imageView), poolIndex(poolIndex) {}

        ~RenderTarget();

        const ImageViewPtr &ImageView() const {
            return imageView;
        }

        const ImageViewPtr &LayerImageView(uint32 layer);

        const RenderTargetDesc &Desc() const {
            return desc;
        }

        bool OwnedByPool() const {
            return poolIndex != ~0u;
        }

    protected:
        int unusedFrames = 0;
        friend class RenderTargetManager;

    private:
        DeviceContext *device;
        RenderTargetDesc desc;
        ImageViewPtr imageView;
        size_t poolIndex;
        vector<ImageViewPtr> layerImageViews;
    };

    class RenderTargetManager {
    public:
        RenderTargetManager(DeviceContext &device) : device(device) {}
        RenderTargetPtr Get(const RenderTargetDesc &desc);
        void TickFrame();

    private:
        DeviceContext &device;

        using RenderTargetKey = HashKey<RenderTargetDesc>;
        robin_hood::unordered_map<RenderTargetKey, vector<RenderTargetPtr>, typename RenderTargetKey::Hasher> pool;
    };
} // namespace sp::vulkan
