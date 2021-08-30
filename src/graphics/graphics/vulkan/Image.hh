#pragma once

#include "Common.hh"

namespace sp::vulkan {
    enum class LoadOp {
        DontCare,
        Clear,
        Load,
    };

    enum class StoreOp {
        DontCare,
        Store,
    };

    // TODO: this is a pretty crappy type that I cobbled together to get exactly
    // the information needed for render pass creation, replace it with something better
    struct ImageView {
        ImageView() {}
        ImageView(const vk::ImageView &view, const vk::ImageViewCreateInfo &info, vk::Extent2D extent)
            : view(view), info(info), extent(extent.width, extent.height, 1) {}
        ImageView(const vk::ImageView &view, const vk::ImageViewCreateInfo &info, vk::Extent3D extent)
            : view(view), info(info), extent(extent) {}

        vk::ImageView view = {};
        vk::ImageViewCreateInfo info;
        vk::Extent3D extent;
        vk::ImageLayout swapchainLayout = vk::ImageLayout::eUndefined;

        bool IsSwapchain() const {
            return swapchainLayout != vk::ImageLayout::eUndefined;
        }
    };

    vk::ImageAspectFlags FormatToAspectFlags(vk::Format format);
} // namespace sp::vulkan
