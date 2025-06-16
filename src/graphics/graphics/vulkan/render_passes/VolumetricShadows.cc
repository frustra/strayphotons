/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Lighting.hh"
#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"

namespace sp::vulkan::renderer {
    static CVar<bool> CVarEnableVolumetricShadows("r.VolumetricShadows", true, "Enable or disable volumnetric shadows");

    void Lighting::AddVolumetricShadows(RenderGraph &graph) {
        if (!CVarEnableVolumetricShadows.Get()) return;

        graph.AddPass("VolumetricShadows")
            .Build([&](rg::PassBuilder &builder) {
                builder.Read("ShadowMap.Linear", Access::FragmentShaderSampleImage);
                builder.SetColorAttachment(0, graph.LastOutputID(), {LoadOp::Load, StoreOp::Store});

                builder.ReadUniform("ViewState");
                builder.ReadUniform("LightState");

                builder.SetDepthAttachment("GBufferDepthStencil", {LoadOp::Load, StoreOp::ReadOnly});
            })
            .Execute([this](rg::Resources &resources, CommandContext &cmd) {
                cmd.SetShaders("shadow_mesh.vert", "shadow_mesh.frag");
                cmd.SetDepthTest(true, false);
                cmd.SetDepthCompareOp(vk::CompareOp::eLessOrEqual);
                cmd.SetBlending(true);
                cmd.SetBlendFunc(vk::BlendFactor::eOne, vk::BlendFactor::eOne);

                cmd.SetImageView("shadowMap", "ShadowMap.Linear");
                cmd.SetUniformBuffer("ViewStates", "ViewState");
                cmd.SetUniformBuffer("LightData", "LightState");

                for (size_t i = 0; i < lights.size(); i++) {
                    cmd.PushConstants((uint32_t)i);
                    cmd.Draw(views[i].extents.x * views[i].extents.y * 12);
                }
            });
    }
} // namespace sp::vulkan::renderer
