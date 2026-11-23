/*
 * Stray Photons - Copyright (C) 2026 Jacob Wirth & Justine Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Screenshots.hh"

#include "ecs/EcsImpl.hh"
#include "graphics/vulkan/Renderer.hh"
#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/core/VkCommon.hh"
#include "graphics/vulkan/render_graph/PooledImage.hh"
#include "graphics/vulkan/render_graph/Resources.hh"
#include "graphics/vulkan/render_passes/Readback.hh"
#include "graphics/vulkan/render_passes/VisualizeBuffer.hh"
#include "strayphotons/Logging.hh"
#include "strayphotons/Utility.hh"
#include "vulkan/vulkan.hpp"

#include <filesystem>
#include <fpng.h>
#include <string>

namespace sp::vulkan::renderer {
    Screenshots::Screenshots() {
        funcs.Register<std::string, std::string>("screenshot",
            "Save screenshot to <path>, optionally specifying an image <resource>",
            [&](std::string path, std::string resource) {
                std::lock_guard lock(screenshotMutex);
                pendingScreenshots.push_back(PendingScreenshot{path, resource, false});
            });
        funcs.Register<std::string, std::string>("assert_screenshot",
            "Save screenshot to <path>, optionally specifying an image <resource>, crash if the resource is missing",
            [&](std::string path, std::string resource) {
                std::lock_guard lock(screenshotMutex);
                pendingScreenshots.push_back(PendingScreenshot{path, resource, true});
            });
    }

    void Screenshots::AddPass(RenderGraph &graph) {
        std::lock_guard lock(screenshotMutex);

        for (PendingScreenshot &pending : pendingScreenshots) {
            std::string screenshotPath = pending.path;
            std::string screenshotResource = pending.resource;
            if (screenshotResource.empty()) screenshotResource = CVarWindowViewTarget.Get();
            if (!starts_with(screenshotResource, "/")) screenshotResource = "/" + screenshotResource;

            rg::ResourceID screenshotID = rg::InvalidResource;
            rg::ResourceID sourceID = rg::InvalidResource;
            ImageDesc screenshotDesc = {};
            graph.AddPass("Screenshot")
                .Build([&](rg::PassBuilder &builder) {
                    auto resourceID = builder.GetID(screenshotResource, pending.assert);
                    if (resourceID == InvalidResource) {
                        Errorf("Can't screenshot \"%s\": invalid resource", screenshotResource);
                        return;
                    }
                    auto resource = builder.GetResource(resourceID);
                    if (resource.type != rg::Resource::Type::Image) {
                        if (pending.assert) {
                            Abortf("Can't screenshot \"%s\": invalid resource type %s",
                                screenshotResource,
                                resource.type);
                        } else {
                            Errorf("Can't screenshot \"%s\": invalid resource type %s",
                                screenshotResource,
                                resource.type);
                        }
                        return;
                    }
                    screenshotDesc = resource.DeriveImage();
                    screenshotDesc.imageType = vk::ImageType::e2D;
                    screenshotDesc.extent.depth = 1;
                    screenshotDesc.mipLevels = 1;
                    screenshotDesc.arrayLayers = 1;
                    if (FormatByteSize(screenshotDesc.format) == FormatComponentCount(screenshotDesc.format)) {
                        sourceID = resource.id;
                        uint32_t components = FormatComponentCount(screenshotDesc.format);
                        if (components == 1) {
                            screenshotDesc.format = vk::Format::eR8Unorm;
                        } else if (components == 2) {
                            screenshotDesc.format = vk::Format::eR8G8Unorm;
                        } else if (components == 3) {
                            screenshotDesc.format = vk::Format::eR8G8B8Unorm;
                        } else if (components == 4) {
                            screenshotDesc.format = vk::Format::eR8G8B8A8Unorm;
                        } else {
                            Abortf("format has unsupported component count: %u", components);
                        }

                    } else {
                        sourceID = VisualizeBuffer(graph, resource.id);
                        screenshotDesc.format = vk::Format::eR8G8B8A8Unorm;
                    }
                    builder.Read(sourceID, Access::TransferRead);
                    screenshotID = builder.CreateImage("", screenshotDesc, Access::TransferWrite).id;
                })
                .Execute([sourceID, screenshotID, screenshotDesc](rg::Resources &resources, CommandContext &cmd) {
                    auto source = resources.GetImageView(sourceID);
                    vk::ImageBlit blit;
                    blit.srcSubresource = {vk::ImageAspectFlagBits::eColor,
                        source->BaseMipLevel(),
                        source->BaseArrayLayer(),
                        1};
                    blit.srcOffsets[0] = vk::Offset3D();
                    blit.srcOffsets[1] = vk::Offset3D(screenshotDesc.extent.width, screenshotDesc.extent.height);
                    blit.dstSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1};
                    blit.dstOffsets[0] = vk::Offset3D();
                    blit.dstOffsets[1] = vk::Offset3D(screenshotDesc.extent.width, screenshotDesc.extent.height);

                    auto output = resources.GetImageView(screenshotID);
                    cmd.Raw().blitImage(*source->Image(),
                        vk::ImageLayout::eTransferSrcOptimal,
                        *output->Image(),
                        vk::ImageLayout::eTransferDstOptimal,
                        {blit},
                        vk::Filter::eNearest);
                });

            if (screenshotID == rg::InvalidResource) continue;

            vk::ImageSubresourceLayers subresource = {};
            subresource.aspectMask = vk::ImageAspectFlagBits::eColor;
            subresource.layerCount = 1;
            AddImageReadback(graph,
                screenshotID,
                subresource,
                {},
                screenshotDesc.extent,
                [screenshotPath, screenshotDesc](BufferPtr buffer) {
                    renderer::WriteScreenshot(screenshotPath, screenshotDesc, buffer);
                });
        }
        pendingScreenshots.clear();
    }

    void WriteScreenshot(const std::string &path, ImageDesc imageDesc, const BufferPtr &buffer) {
        auto base = std::filesystem::absolute("screenshots");
        if (!std::filesystem::is_directory(base)) {
            if (!std::filesystem::create_directory(base)) {
                Errorf("Couldn't save screenshot, couldn't create output directory: %s", base.c_str());
                return;
            }
        }
        imageDesc.extent.depth = 1;

        auto fullPath = std::filesystem::weakly_canonical(base / path);
        Logf("Saving %ux%u screenshot to: %s", imageDesc.extent.width, imageDesc.extent.height, fullPath.string());

        uint32_t components = FormatComponentCount(imageDesc.format);
        Assert(FormatByteSize(imageDesc.format) == components, "format must have 1 byte per component");

        static bool fpng_init = false;
        if (!fpng_init) {
            fpng::fpng_init();
            fpng_init = true;
        }

        uint8_t *data = nullptr;
        buffer->Map((void **)&data);

        fpng::fpng_encode_image_to_file(fullPath.string().c_str(),
            data,
            imageDesc.extent.width,
            imageDesc.extent.height,
            components,
            fpng::FPNG_ENCODE_SLOWER); // FPNG_ENCODE_SLOWER = 2-pass compression for smaller files
        buffer->Unmap();
    }
} // namespace sp::vulkan::renderer
