/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Blur.hh"

#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/render_graph/Resources.hh"

namespace sp::vulkan::renderer {
    ResourceID AddGaussianBlur1D(RenderGraph &graph,
        ResourceID sourceID,
        glm::ivec2 direction,
        uint32 downsample,
        float scale,
        float clip) {

        struct {
            glm::vec2 direction;
            float threshold;
            float scale;
        } constants;

        constants.direction = glm::vec2(direction);
        constants.threshold = clip;
        constants.scale = scale;

        return graph.AddPass("GaussianBlur")
            .Build([&](PassBuilder &builder) {
                builder.Read(sourceID, Access::FragmentShaderSampleImage);

                auto desc = builder.DeriveImage(sourceID);
                desc.extent.width = std::max(desc.extent.width / downsample, 1u);
                desc.extent.height = std::max(desc.extent.height / downsample, 1u);
                builder.OutputColorAttachment(0, "", desc, {LoadOp::DontCare, StoreOp::Store});
            })
            .Execute([sourceID, constants](Resources &resources, CommandContext &cmd) {
                auto source = resources.GetImageView(sourceID);

                if (source->ViewType() == vk::ImageViewType::e2DArray) {
                    cmd.SetShaders("screen_cover.vert", "gaussian_blur_array.frag");
                } else {
                    cmd.SetShaders("screen_cover.vert", "gaussian_blur.frag");
                }

                cmd.SetImageView("sourceTex", source);
                cmd.PushConstants(constants);
                cmd.Draw(3);
            });
    }

    void AddBackgroundBlur(RenderGraph &graph) {
        auto scope = graph.Scope("BackgroundBlur");

        auto inputID = graph.LastOutputID();
        renderer::AddGaussianBlur1D(graph, inputID, glm::ivec2(0, 1), 2);
        renderer::AddGaussianBlur1D(graph, graph.LastOutputID(), glm::ivec2(1, 0), 2);
        renderer::AddGaussianBlur1D(graph, graph.LastOutputID(), glm::ivec2(0, 1), 1);
        renderer::AddGaussianBlur1D(graph, graph.LastOutputID(), glm::ivec2(1, 0), 2);
        renderer::AddGaussianBlur1D(graph, graph.LastOutputID(), glm::ivec2(0, 1), 1);
        renderer::AddGaussianBlur1D(graph, graph.LastOutputID(), glm::ivec2(1, 0), 1, 0.2f);

        graph.AddPass("Resize")
            .Build([&](PassBuilder &builder) {
                builder.Read(graph.LastOutputID(), Access::FragmentShaderSampleImage);

                builder.SetColorAttachment(0, inputID, {LoadOp::DontCare, StoreOp::Store});
            })
            .Execute([](Resources &resources, CommandContext &cmd) {
                auto view = resources.GetImageView(resources.LastOutputID());
                cmd.DrawScreenCover(view);
            });
    }
} // namespace sp::vulkan::renderer
