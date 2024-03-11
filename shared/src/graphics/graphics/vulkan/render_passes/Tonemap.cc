/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Tonemap.hh"

#include "graphics/vulkan/core/CommandContext.hh"

namespace sp::vulkan::renderer {
    void AddTonemap(RenderGraph &graph) {
        graph.AddPass("Tonemap")
            .Build([&](rg::PassBuilder &builder) {
                auto luminanceID = builder.LastOutputID();
                builder.Read(luminanceID, Access::FragmentShaderSampleImage);

                auto desc = builder.DeriveImage(luminanceID);
                desc.format = vk::Format::eR8G8B8A8Srgb;
                builder.OutputColorAttachment(0, "TonemappedLuminance", desc, {LoadOp::DontCare, StoreOp::Store});
            })
            .Execute([](rg::Resources &resources, CommandContext &cmd) {
                cmd.SetShaders("screen_cover.vert", "tonemap.frag");
                cmd.SetImageView(0, 0, resources.GetImageView(resources.LastOutputID()));
                cmd.Draw(3);
            });
    }
} // namespace sp::vulkan::renderer
