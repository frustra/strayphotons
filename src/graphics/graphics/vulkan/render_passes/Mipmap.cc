#include "Mipmap.hh"

#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"

namespace sp::vulkan::renderer {
    void AddMipmap(RenderGraph &graph, ResourceID id) {
        graph.AddPass("Mipmap")
            .Build([&](rg::PassBuilder &builder) {
                builder.Write(id, Access::TransferWrite);
                builder.Read(id, Access::TransferRead);
            })
            .Execute([id](rg::Resources &resources, CommandContext &cmd) {
                const auto &image = resources.GetImageView(id)->Image();

                ImageBarrierInfo transferMips;
                transferMips.trackImageLayout = false;
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

                vk::Offset3D currentExtent = {(int32)baseMipExtent.width,
                    (int32)baseMipExtent.height,
                    (int32)baseMipExtent.depth};

                for (uint32 i = 1; i < image->MipLevels(); i++) {
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
