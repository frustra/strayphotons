#pragma once

#include "Common.hh"
#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"

namespace sp::vulkan::renderer {
    /**
     * Copies a buffer to the CPU.
     * After the copy is done, at the end of a frame, the callback is called with a BufferPtr containing the data.
     * The buffer will be mappable for access on the CPU.
     */
    template<typename Callback, typename ResourceName>
    void AddBufferReadback(RenderGraph &graph,
        ResourceName resourceID,
        vk::DeviceSize srcOffset,
        vk::DeviceSize size, // pass 0 to copy until the end of the buffer
        Callback &&callback) {

        ResourceID readbackID = InvalidResource;
        vk::BufferCopy region;
        region.srcOffset = srcOffset;
        region.dstOffset = 0;
        region.size = size;

        graph.AddPass("TransferForBufferReadback")
            .Build([&](rg::PassBuilder &builder) {
                auto resource = builder.GetResource(resourceID);
                Assert(resource.type == Resource::Type::Buffer, "resource must be a buffer");
                builder.Read(resource.id, Access::TransferRead);

                if (region.size == 0) region.size = resource.BufferSize() - region.srcOffset;
                readbackID = builder.CreateBuffer(region.size, Residency::GPU_TO_CPU, Access::TransferWrite).id;
            })
            .Execute([resourceID, readbackID, region](rg::Resources &resources, CommandContext &cmd) {
                auto srcBuffer = resources.GetBuffer(resourceID);
                auto dstBuffer = resources.GetBuffer(readbackID);
                cmd.Raw().copyBuffer(*srcBuffer, *dstBuffer, {region});
            });

        graph.AddPass("BufferReadback")
            .Build([&](rg::PassBuilder &builder) {
                builder.RequirePass();
                builder.Read(readbackID, Access::HostRead);
            })
            .Execute([readbackID, callback](rg::Resources &resources, DeviceContext &device) {
                auto buffer = resources.GetBuffer(readbackID);
                device.ExecuteAfterFrameFence([callback, buffer]() {
                    callback(buffer);
                });
            });
    }

    /**
     * Copies an image to the CPU.
     * After the copy is done, at the end of a frame, the callback is called with a BufferPtr containing the data.
     * The buffer will be mappable for access on the CPU.
     */
    template<typename Callback, typename ResourceName>
    void AddImageReadback(RenderGraph &graph,
        ResourceName resourceID,
        vk::ImageSubresourceLayers subresource, // pass {} for all layers
        vk::Offset3D offset, // pass {} for no offset
        vk::Extent3D extent, // pass {} to copy from the offset to the image's full extent
        Callback &&callback) {

        ResourceID readbackID = InvalidResource;
        vk::BufferImageCopy region;
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource = subresource;
        region.imageOffset = offset;
        region.imageExtent = extent;

        graph.AddPass("TransferForImageReadback")
            .Build([&](rg::PassBuilder &builder) {
                auto resource = builder.GetResource(resourceID);
                Assert(resource.type == Resource::Type::Image, "resource must be an image");
                builder.Read(resource.id, Access::TransferRead);

                if (region.imageExtent == vk::Extent3D{}) {
                    region.imageExtent = resource.ImageExtents();
                    region.imageExtent.width -= region.imageOffset.x;
                    region.imageExtent.height -= region.imageOffset.y;
                    region.imageExtent.depth -= region.imageOffset.z;
                }

                if (region.imageSubresource.layerCount == 0) {
                    region.imageSubresource.layerCount = resource.ImageLayers();
                }

                if (!region.imageSubresource.aspectMask) {
                    region.imageSubresource.aspectMask = FormatToAspectFlags(resource.ImageFormat());
                }

                auto texels = region.imageExtent.width * region.imageExtent.height * region.imageExtent.depth *
                              region.imageSubresource.layerCount;

                auto bufferSize = FormatByteSize(resource.ImageFormat()) * texels;
                readbackID = builder.CreateBuffer(bufferSize, Residency::GPU_TO_CPU, Access::TransferWrite).id;
            })
            .Execute([resourceID, readbackID, region](rg::Resources &resources, CommandContext &cmd) {
                auto srcImage = resources.GetImageView(resourceID)->Image();
                auto dstBuffer = resources.GetBuffer(readbackID);
                cmd.Raw().copyImageToBuffer(*srcImage, srcImage->LastLayout(), *dstBuffer, {region});
            });

        graph.AddPass("ImageReadback")
            .Build([&](rg::PassBuilder &builder) {
                builder.RequirePass();
                builder.Read(readbackID, Access::HostRead);
            })
            .Execute([readbackID, callback](rg::Resources &resources, DeviceContext &device) {
                auto buffer = resources.GetBuffer(readbackID);
                device.ExecuteAfterFrameFence([callback, buffer]() {
                    callback(buffer);
                });
            });
    }
} // namespace sp::vulkan::renderer
