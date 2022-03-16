#include "RenderTarget.hh"

#include "core/Tracing.hh"
#include "graphics/vulkan/core/DeviceContext.hh"

namespace sp::vulkan {
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
} // namespace sp::vulkan
