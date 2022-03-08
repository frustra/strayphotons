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
        info.viewType = desc.DeriveViewType();
        info.baseArrayLayer = layer;
        info.arrayLayerCount = 1;
        view = device->CreateImageView(info);
        return view;
    }

    RenderTargetPtr RenderTargetManager::Get(const RenderTargetDesc &desc) {
        auto &list = pool[desc];
        for (auto &elemRef : list) {
            if (elemRef.use_count() <= 1 && elemRef->desc == desc) {
                elemRef->unusedFrames = 0;
                return elemRef;
            }
        }

        auto createDesc = desc;
        ZoneScopedN("RenderTargetCreate");
        ZoneValue(pool.size());
        ZonePrintf("size=%dx%dx%d", createDesc.extent.width, desc.extent.height, desc.extent.depth);

        Assertf(createDesc.extent.width > 0 && desc.extent.height > 0 && desc.extent.depth > 0,
            "image must not have any zero extents, have %dx%dx%d",
            createDesc.extent.width,
            createDesc.extent.height,
            createDesc.extent.depth);

        if (createDesc.primaryViewType == vk::ImageViewType::e2D)
            createDesc.primaryViewType = createDesc.DeriveViewType();

        ImageCreateInfo imageInfo;
        imageInfo.imageType = createDesc.imageType;
        imageInfo.extent = createDesc.extent;
        imageInfo.arrayLayers = createDesc.arrayLayers;
        imageInfo.format = createDesc.format;
        imageInfo.usage = createDesc.usage;

        ImageViewCreateInfo viewInfo;
        viewInfo.viewType = createDesc.primaryViewType;
        viewInfo.defaultSampler = device.GetSampler(createDesc.sampler);

        auto imageView = device.CreateImageAndView(imageInfo, viewInfo)->Get();
        auto ptr = make_shared<RenderTarget>(device, createDesc, imageView, pool.size());

        list.push_back(ptr);
        return ptr;
    }

    void RenderTargetManager::TickFrame() {
        for (auto it = pool.begin(); it != pool.end();) {
            auto &list = it->second;
            erase_if(list, [&](auto &elemRef) {
                if (elemRef.use_count() > 1) {
                    elemRef->unusedFrames = 0;
                    return false;
                }
                return elemRef->unusedFrames++ > 4;
            });
            if (list.empty()) {
                it = pool.erase(it);
            } else {
                it++;
            }
        }
    }
} // namespace sp::vulkan
