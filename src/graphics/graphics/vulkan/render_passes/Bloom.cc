/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Bloom.hh"

#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/render_passes/Blur.hh"

namespace sp::vulkan::renderer {
    static CVar<float> CVarBloomScale("r.BloomScale", 0.15f, "Bloom scale");

    void AddBloom(RenderGraph &graph) {
        float bloomScale = CVarBloomScale.Get();
        if (bloomScale <= 0.0f) return;

        auto sourceID = graph.LastOutputID();

        graph.BeginScope("BloomBlur");
        auto blurY1 = renderer::AddGaussianBlur1D(graph, sourceID, glm::ivec2(0, 1), 1, bloomScale);
        auto blur1 = renderer::AddGaussianBlur1D(graph, blurY1, glm::ivec2(1, 0), 2);
        auto blurY2 = renderer::AddGaussianBlur1D(graph, blur1, glm::ivec2(0, 1), 1);
        auto blur2 = renderer::AddGaussianBlur1D(graph, blurY2, glm::ivec2(1, 0), 1);
        graph.EndScope();

        graph.AddPass("BloomCombine")
            .Build([&](rg::PassBuilder &builder) {
                builder.Read(sourceID, Access::FragmentShaderSampleImage);
                auto desc = builder.DeriveImage(sourceID);
                builder.OutputColorAttachment(0, "Bloom", desc, {LoadOp::DontCare, StoreOp::Store});

                builder.Read(blur1, Access::FragmentShaderSampleImage);
                builder.Read(blur2, Access::FragmentShaderSampleImage);
            })
            .Execute([sourceID, blur1, blur2](rg::Resources &resources, CommandContext &cmd) {
                cmd.SetShaders("screen_cover.vert", "bloom_combine.frag");
                cmd.SetImageView("luminanceTex", sourceID);
                cmd.SetImageView("blurTex1", blur1);
                cmd.SetImageView("blurTex2", blur2);
                cmd.Draw(3);
            });
    }
} // namespace sp::vulkan::renderer
