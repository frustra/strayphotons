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
    static CVar<float> CVarVolumetricShadowTransmittance("r.VolumetricShadowTransmittance",
        0.01,
        "Amount of light redirected by volumetric fog");

    void Lighting::AddVolumetricShadows(RenderGraph &graph) {
        if (!CVarEnableVolumetricShadows.Get()) return;

        auto inputId = graph.LastOutputID();
        auto outputDesc = graph.LastOutput().DeriveImage();
        float transmittance = CVarVolumetricShadowTransmittance.Get();

        graph.AddPass("VolumetricShadows")
            .Build([&](rg::PassBuilder &builder) {
                builder.Read("ShadowMap.Linear", Access::FragmentShaderSampleImage);

                builder.ReadUniform("ViewState");
                builder.ReadUniform("LightState");

                ImageDesc desc = outputDesc;
                desc.format = vk::Format::eR32Sfloat;
                builder.OutputColorAttachment(0, "VolumetricAccumulate", desc, {LoadOp::Clear, StoreOp::Store});
                builder.SetDepthAttachment("GBufferDepthStencil", {LoadOp::Load, StoreOp::ReadOnly});
            })
            .Execute([this, transmittance](rg::Resources &resources, CommandContext &cmd) {
                cmd.SetShaders("shadow_mesh.vert", "shadow_mesh.frag");
                cmd.SetDepthTest(true, false);
                cmd.SetDepthCompareOp(vk::CompareOp::eLessOrEqual);
                cmd.SetBlending(true);
                cmd.SetBlendFunc(vk::BlendFactor::eOne, vk::BlendFactor::eOne);

                cmd.SetImageView("shadowMap", "ShadowMap.Linear");
                cmd.SetUniformBuffer("ViewStates", "ViewState");
                cmd.SetUniformBuffer("LightData", "LightState");

                for (size_t i = 0; i < lights.size(); i++) {
                    struct {
                        uint32_t lightIndex;
                        float transmittance;
                    } pushConstants = {(uint32_t)i, transmittance};
                    cmd.PushConstants(pushConstants);
                    cmd.Draw(views[i].extents.x * views[i].extents.y * 12);
                }
            });

        graph.AddPass("CompositeVolumetric")
            .Build([&](rg::PassBuilder &builder) {
                builder.Read("VolumetricAccumulate", Access::FragmentShaderSampleImage);
                builder.Read(inputId, Access::FragmentShaderSampleImage);
                builder.Read("GBufferDepthStencil", Access::FragmentShaderSampleImage);

                builder.SetColorAttachment(0, inputId, {LoadOp::Load, StoreOp::Store});
            })
            .Execute([this, inputId, transmittance](rg::Resources &resources, CommandContext &cmd) {
                cmd.SetShaders("screen_cover.vert", "shadow_mesh_blend.frag");
                cmd.SetUniformBuffer("ViewStates", "ViewState");
                cmd.SetBlending(true);
                cmd.SetBlendFunc(vk::BlendFactor::eOne, vk::BlendFactor::eOne);
                cmd.SetImageView("tex", "VolumetricAccumulate");
                cmd.SetImageView("gBufferDepth", resources.GetImageDepthView("GBufferDepthStencil"));
                cmd.PushConstants(transmittance);
                cmd.Draw(3);
            });
    }
} // namespace sp::vulkan::renderer
