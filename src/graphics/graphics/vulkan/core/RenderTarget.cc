#include "RenderTarget.hh"

#include "core/Tracing.hh"
#include "graphics/vulkan/core/DeviceContext.hh"

namespace sp::vulkan {
    RenderTarget::~RenderTarget() {
        if (poolIndex != ~0u) {
            ZoneScoped;
            ZoneValue(poolIndex);
            ZonePrintf("size=%dx%d", desc.extent.width, desc.extent.height);
        }
    }

    const ImageViewPtr &RenderTarget::LayerImageView(uint32 layer) {
        Assert(layer < desc.arrayLayers, "render target image layer too high");
        if (layerImageViews.empty()) layerImageViews.resize(desc.arrayLayers);

        auto &view = layerImageViews[layer];
        if (view) return view;

        ImageViewCreateInfo info = imageView->CreateInfo();
        info.viewType = vk::ImageViewType::e2D;
        info.baseArrayLayer = layer;
        info.arrayLayerCount = 1;
        view = device->CreateImageView(info);
        return view;
    }

    RenderTargetPtr RenderTargetManager::Get(const RenderTargetDesc &desc) {
        for (auto &elemRef : pool) {
            if (elemRef.use_count() <= 1 && elemRef->desc == desc) {
                elemRef->unusedFrames = 0;
                return elemRef;
            }
        }

        ZoneScopedN("RenderTargetCreate");
        ZoneValue(pool.size());
        ZonePrintf("size=%dx%dx%d", desc.extent.width, desc.extent.height, desc.extent.depth);

        ImageCreateInfo imageInfo;
        imageInfo.imageType = desc.imageType;
        imageInfo.extent = desc.extent;
        imageInfo.arrayLayers = desc.arrayLayers;
        imageInfo.format = desc.format;
        imageInfo.usage = desc.usage;

        ImageViewCreateInfo viewInfo;
        viewInfo.viewType = desc.primaryViewType;
        viewInfo.defaultSampler = device.GetSampler(SamplerType::BilinearClamp);

        auto imageView = device.CreateImageAndView(imageInfo, viewInfo)->Get();
        auto ptr = make_shared<RenderTarget>(device, desc, imageView, pool.size());

        pool.push_back(ptr);
        return ptr;
    }

    void RenderTargetManager::TickFrame() {
        erase_if(pool, [&](auto &elemRef) {
            if (elemRef.use_count() > 1) {
                elemRef->unusedFrames = 0;
                return false;
            }
            return elemRef->unusedFrames++ > 4;
        });
    }
} // namespace sp::vulkan
