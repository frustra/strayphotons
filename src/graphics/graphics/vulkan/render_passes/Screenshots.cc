#include "Screenshots.hh"

#include "core/Logging.hh"
#include "graphics/vulkan/Renderer.hh"
#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/render_passes/VisualizeBuffer.hh"

#include <filesystem>
#include <stb_image_write.h>

namespace sp::vulkan::renderer {
    Screenshots::Screenshots() {
        funcs.Register<string, string>("screenshot",
            "Save screenshot to <path>, optionally specifying an image <resource>",
            [&](string path, string resource) {
                std::lock_guard lock(screenshotMutex);
                pendingScreenshots.push_back({path, resource});
            });
    }

    void Screenshots::AddPass(RenderGraph &graph) {
        std::lock_guard lock(screenshotMutex);

        for (auto &pending : pendingScreenshots) {
            auto screenshotPath = pending.first;
            auto screenshotResource = pending.second;
            if (screenshotResource.empty()) screenshotResource = CVarWindowViewTarget.Get();

            rg::ResourceID sourceID = rg::InvalidResource;

            graph.AddPass("Screenshot")
                .Build([&](rg::PassBuilder &builder) {
                    auto resource = builder.GetResource(screenshotResource);
                    if (resource.type != rg::Resource::Type::RenderTarget) {
                        Errorf("Can't screenshot \"%s\": invalid resource", screenshotResource);
                    } else {
                        auto format = resource.RenderTargetFormat();
                        if (FormatByteSize(format) == FormatComponentCount(format)) {
                            sourceID = resource.id;
                        } else {
                            sourceID = VisualizeBuffer(graph, resource.id);
                        }
                        builder.TransferRead(sourceID);
                        builder.RequirePass();
                    }
                })
                .Execute([screenshotPath, sourceID](rg::Resources &resources, DeviceContext &device) {
                    auto &res = resources.GetResource(sourceID);
                    if (res.type == rg::Resource::Type::RenderTarget) {
                        auto target = resources.GetRenderTarget(res.id);
                        renderer::WriteScreenshot(device, screenshotPath, target->ImageView());
                    }
                });
        }
        pendingScreenshots.clear();
    }

    void WriteScreenshot(DeviceContext &device, const std::string &path, const ImageViewPtr &view) {
        auto base = std::filesystem::absolute("screenshots");
        if (!std::filesystem::is_directory(base)) {
            if (!std::filesystem::create_directory(base)) {
                Errorf("Couldn't save screenshot, couldn't create output directory: %s", base.c_str());
                return;
            }
        }
        auto fullPath = std::filesystem::weakly_canonical(base / path);
        Logf("Saving screenshot to: %s", fullPath.string());

        auto extent = view->Extent();
        extent.depth = 1;

        vk::ImageCreateInfo outputDesc;
        outputDesc.imageType = vk::ImageType::e2D;
        outputDesc.extent = extent;
        outputDesc.usage = vk::ImageUsageFlagBits::eTransferDst;
        outputDesc.mipLevels = 1;
        outputDesc.arrayLayers = 1;
        outputDesc.tiling = vk::ImageTiling::eLinear;

        uint32 components = FormatComponentCount(view->Format());
        if (components == 1) {
            outputDesc.format = vk::Format::eR8Srgb;
        } else if (components == 2) {
            outputDesc.format = vk::Format::eR8G8Srgb;
        } else if (components == 3) {
            outputDesc.format = vk::Format::eR8G8B8Srgb;
        } else if (components == 4) {
            outputDesc.format = vk::Format::eR8G8B8A8Srgb;
        } else {
            Abortf("format has unsupported component count: %u", components);
        }

        Assert(FormatByteSize(view->Format()) == FormatByteSize(outputDesc.format),
            "format must have 1 byte per component");

        auto outputImage = device.AllocateImage(outputDesc, VMA_MEMORY_USAGE_GPU_TO_CPU);

        auto transferCmd = device.GetFencedCommandContext(CommandContextType::General);
        transferCmd->ImageBarrier(outputImage,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eTransferDstOptimal,
            vk::PipelineStageFlagBits::eTransfer,
            {},
            vk::PipelineStageFlagBits::eTransfer,
            vk::AccessFlagBits::eTransferWrite);

        auto lastLayout = view->Image()->LastLayout();
        if (lastLayout != vk::ImageLayout::eTransferSrcOptimal) {
            transferCmd->ImageBarrier(view->Image(),
                lastLayout,
                vk::ImageLayout::eTransferSrcOptimal,
                vk::PipelineStageFlagBits::eTransfer,
                {},
                vk::PipelineStageFlagBits::eTransfer,
                vk::AccessFlagBits::eTransferWrite);
        }

        vk::ImageCopy imageCopyRegion;
        imageCopyRegion.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        imageCopyRegion.srcSubresource.mipLevel = view->BaseMipLevel();
        imageCopyRegion.srcSubresource.baseArrayLayer = view->BaseArrayLayer();
        imageCopyRegion.srcSubresource.layerCount = 1;
        imageCopyRegion.dstSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1};
        imageCopyRegion.extent = extent;

        transferCmd->Raw().copyImage(*view->Image(),
            vk::ImageLayout::eTransferSrcOptimal,
            *outputImage,
            vk::ImageLayout::eTransferDstOptimal,
            {imageCopyRegion});

        transferCmd->ImageBarrier(outputImage,
            vk::ImageLayout::eTransferDstOptimal,
            vk::ImageLayout::eGeneral,
            vk::PipelineStageFlagBits::eTransfer,
            vk::AccessFlagBits::eTransferWrite,
            vk::PipelineStageFlagBits::eTransfer,
            vk::AccessFlagBits::eMemoryRead);

        if (lastLayout != vk::ImageLayout::eTransferSrcOptimal) {
            transferCmd->ImageBarrier(view->Image(),
                vk::ImageLayout::eTransferSrcOptimal,
                lastLayout,
                vk::PipelineStageFlagBits::eTransfer,
                vk::AccessFlagBits::eTransferWrite,
                vk::PipelineStageFlagBits::eTransfer,
                vk::AccessFlagBits::eMemoryRead);
        }

        auto fence = transferCmd->Fence();
        device.Submit(transferCmd, {}, {}, {});

        AssertVKSuccess(device->waitForFences({fence}, true, 1e10), "waiting for fence");

        vk::ImageSubresource subResource = {vk::ImageAspectFlagBits::eColor, 0, 0};
        auto subResourceLayout = device->getImageSubresourceLayout(*outputImage, subResource);

        uint8 *data;
        outputImage->Map((void **)&data);
        stbi_write_png((const char *)fullPath.string().c_str(),
            extent.width,
            extent.height,
            components,
            data + subResourceLayout.offset,
            subResourceLayout.rowPitch);
        outputImage->Unmap();
    }
} // namespace sp::vulkan::renderer
