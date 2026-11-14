/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justine Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Screenshots.hh"

#include "ecs/EcsImpl.hh"
#include "graphics/vulkan/Renderer.hh"
#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/core/Memory.hh"
#include "graphics/vulkan/core/VkCommon.hh"
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

            rg::ResourceID sourceID = rg::InvalidResource;
            vk::Extent3D extent = {};
            vk::Format format = vk::Format::eUndefined;
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
                    } else {
                        extent = resource.ImageExtents();
                        format = resource.ImageFormat();
                        if (FormatByteSize(format) == FormatComponentCount(format)) {
                            sourceID = resource.id;
                        } else {
                            sourceID = VisualizeBuffer(graph, resource.id);
                            format = vk::Format::eR8G8B8A8Srgb;
                        }
                    }
                })
                .Execute([](rg::Resources &resources, DeviceContext &device) {});

            if (sourceID == rg::InvalidResource) continue;

            vk::ImageSubresourceLayers subresource = {};
            subresource.aspectMask = vk::ImageAspectFlagBits::eColor;
            subresource.layerCount = 1;
            AddImageReadback(graph,
                sourceID,
                subresource,
                {},
                extent,
                [screenshotPath, extent, format](BufferPtr buffer) {
                    renderer::WriteScreenshot(screenshotPath, extent, format, buffer);
                });
        }
        pendingScreenshots.clear();
    }

    void WriteScreenshot(const std::string &path, vk::Extent3D extent, vk::Format format, const BufferPtr &buffer) {
        auto base = std::filesystem::absolute("screenshots");
        if (!std::filesystem::is_directory(base)) {
            if (!std::filesystem::create_directory(base)) {
                Errorf("Couldn't save screenshot, couldn't create output directory: %s", base.c_str());
                return;
            }
        }
        extent.depth = 1;

        auto fullPath = std::filesystem::weakly_canonical(base / path);
        Logf("Saving %ux%u screenshot to: %s", extent.width, extent.height, fullPath.string());

        vk::ImageCreateInfo outputDesc = {};
        outputDesc.imageType = vk::ImageType::e2D;
        outputDesc.extent = extent;
        outputDesc.usage = vk::ImageUsageFlagBits::eTransferDst;
        outputDesc.mipLevels = 1;
        outputDesc.arrayLayers = 1;
        outputDesc.tiling = vk::ImageTiling::eLinear;

        uint32_t components = FormatComponentCount(format);
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

        Assert(FormatByteSize(format) == FormatByteSize(outputDesc.format), "format must have 1 byte per component");

        static bool fpng_init = false;
        if (!fpng_init) {
            fpng::fpng_init();
            fpng_init = true;
        }

        uint8_t *data = nullptr;
        buffer->Map((void **)&data);

        fpng::fpng_encode_image_to_file(fullPath.string().c_str(),
            data,
            extent.width,
            extent.height,
            components,
            fpng::FPNG_ENCODE_SLOWER); // FPNG_ENCODE_SLOWER = 2-pass compression for smaller files
        buffer->Unmap();
    }
} // namespace sp::vulkan::renderer
