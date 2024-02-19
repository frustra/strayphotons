/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "PooledImage.hh"

#include "common/Tracing.hh"
#include "graphics/vulkan/core/DeviceContext.hh"

namespace sp::vulkan::render_graph {
    const ImageViewPtr &PooledImage::LayerImageView(uint32 layer) {
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

    const ImageViewPtr &PooledImage::MipImageView(uint32 mip) {
        Assert(mip < desc.mipLevels, "render target image layer too high");
        if (mipImageViews.empty()) mipImageViews.resize(desc.mipLevels);

        auto &view = mipImageViews[mip];
        if (view) return view;

        ImageViewCreateInfo info = imageView->CreateInfo();
        info.viewType = desc.DeriveViewType();
        info.baseMipLevel = mip;
        info.mipLevelCount = 1;
        view = device->CreateImageView(info);
        return view;
    }

    const ImageViewPtr &PooledImage::DepthImageView() {
        if (depthImageView) return depthImageView;

        ImageViewCreateInfo info = imageView->CreateInfo();
        info.aspectMask = vk::ImageAspectFlagBits::eDepth;
        depthImageView = device->CreateImageView(info);
        return depthImageView;
    }
} // namespace sp::vulkan::render_graph
