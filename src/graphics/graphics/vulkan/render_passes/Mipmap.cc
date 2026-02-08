/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Mipmap.hh"

#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/render_graph/Resources.hh"

namespace sp::vulkan::renderer {
    void AddMipmap(RenderGraph &graph, ResourceID id) {
        if (id == InvalidResource) id = graph.LastOutputID();
        graph.AddPass("Mipmap")
            .Build([&](rg::PassBuilder &builder) {
                builder.Write(id, Access::TransferWrite);
                builder.Read(id, Access::TransferRead);
            })
            .Execute([id](rg::Resources &resources, CommandContext &cmd) {
                const auto &image = resources.GetImageView(id)->Image();

                ImageBarrierInfo transferMips = {};
                transferMips.trackImageLayout = false;
                transferMips.baseMipLevel = 0;
                transferMips.mipLevelCount = 1;

                cmd.ImageBarrier(image,
                    vk::ImageLayout::eUndefined,
                    vk::ImageLayout::eTransferSrcOptimal,
                    vk::PipelineStageFlagBits::eTopOfPipe,
                    {},
                    vk::PipelineStageFlagBits::eTransfer,
                    vk::AccessFlagBits::eTransferRead,
                    transferMips);

                transferMips.baseMipLevel = 1;
                transferMips.mipLevelCount = image->MipLevels() - 1;

                cmd.ImageBarrier(image,
                    vk::ImageLayout::eUndefined,
                    vk::ImageLayout::eTransferDstOptimal,
                    vk::PipelineStageFlagBits::eTopOfPipe,
                    {},
                    vk::PipelineStageFlagBits::eTransfer,
                    vk::AccessFlagBits::eTransferWrite,
                    transferMips);

                auto baseMipExtent = image->Extent();

                vk::Offset3D currentExtent = {(int32_t)baseMipExtent.width,
                    (int32_t)baseMipExtent.height,
                    (int32_t)baseMipExtent.depth};

                for (uint32_t i = 1; i < image->MipLevels(); i++) {
                    auto prevMipExtent = currentExtent;
                    currentExtent.x = std::max(currentExtent.x >> 1, 1);
                    currentExtent.y = std::max(currentExtent.y >> 1, 1);
                    currentExtent.z = std::max(currentExtent.z >> 1, 1);

                    vk::ImageBlit blit;
                    blit.srcSubresource = {vk::ImageAspectFlagBits::eColor, i - 1, 0, 1};
                    blit.srcOffsets[0] = vk::Offset3D();
                    blit.srcOffsets[1] = prevMipExtent;
                    blit.dstSubresource = {vk::ImageAspectFlagBits::eColor, i, 0, 1};
                    blit.dstOffsets[0] = vk::Offset3D();
                    blit.dstOffsets[1] = currentExtent;

                    cmd.Raw().blitImage(*image,
                        vk::ImageLayout::eTransferSrcOptimal,
                        *image,
                        vk::ImageLayout::eTransferDstOptimal,
                        {blit},
                        vk::Filter::eLinear);

                    transferMips.baseMipLevel = i;
                    transferMips.mipLevelCount = 1;
                    cmd.ImageBarrier(image,
                        vk::ImageLayout::eTransferDstOptimal,
                        vk::ImageLayout::eTransferSrcOptimal,
                        vk::PipelineStageFlagBits::eTransfer,
                        vk::AccessFlagBits::eTransferWrite,
                        vk::PipelineStageFlagBits::eTransfer,
                        vk::AccessFlagBits::eTransferRead,
                        transferMips);
                }

                // Each mip has now been transitioned to TransferSrc.
                image->SetLayout(vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferSrcOptimal);
            });
    }
} // namespace sp::vulkan::renderer
