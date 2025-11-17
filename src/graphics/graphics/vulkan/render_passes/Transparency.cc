/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Transparency.hh"

#include "ecs/EcsImpl.hh"
#include "ecs/components/View.hh"
#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/core/PerfTimer.hh"
#include "graphics/vulkan/render_passes/Lighting.hh"
#include "graphics/vulkan/render_passes/Voxels.hh"

namespace sp::vulkan::renderer {
    void Transparency::AddPass(RenderGraph &graph, const ecs::View &view) {
        glm::vec3 viewPos = view.invViewMat * glm::vec4(0, 0, 0, 1);
        auto drawIDs = scene.GenerateSortedDrawsForView(graph, viewPos, ecs::VisibilityMask::Transparent, true);
        uint32_t voxelLayerCount = std::min(CVarLightingVoxelLayers.Get(), voxels.GetLayerCount());

        graph.AddPass("Transparency")
            .Build([&](PassBuilder &builder) {
                builder.Read("ExposureState", Access::FragmentShaderReadStorage);
                builder.Read("ShadowMap.Linear", Access::FragmentShaderSampleImage);
                builder.Read("Voxels.Radiance", Access::FragmentShaderSampleImage);
                builder.Read("Voxels.Normals", Access::FragmentShaderSampleImage);

                for (auto &voxelLayer : Voxels::VoxelLayers[voxelLayerCount - 1]) {
                    builder.Read(voxelLayer.fullName, Access::FragmentShaderSampleImage);
                }

                builder.ReadUniform("ViewState");
                builder.ReadUniform("LightState");
                builder.ReadUniform("VoxelState");

                builder.Read("WarpedVertexBuffer", Access::VertexBuffer);
                builder.Read(drawIDs.drawCommandsBuffer, Access::IndirectBuffer);
                builder.Read(drawIDs.drawParamsBuffer, Access::VertexShaderReadStorage);

                builder.SetColorAttachment(0, builder.LastOutputID(), {LoadOp::Load, StoreOp::Store});
                builder.SetDepthAttachment("GBufferDepthStencil", {LoadOp::Load, StoreOp::ReadOnly});
            })
            .Execute([this, drawIDs, voxelLayerCount](Resources &resources, CommandContext &cmd) {
                cmd.SetShaders("scene.vert", "lighting_transparent.frag");

                cmd.SetShaderConstant(ShaderStage::Fragment, "LIGHTING_MODE", CVarLightingMode.Get());
                cmd.SetShaderConstant(ShaderStage::Fragment, "SHADOW_MAP_SAMPLE_WIDTH", CVarShadowMapSampleWidth.Get());
                cmd.SetShaderConstant(ShaderStage::Fragment, "SHADOW_MAP_SAMPLE_COUNT", CVarShadowMapSampleCount.Get());
                cmd.SetShaderConstant(ShaderStage::Fragment,
                    "ENABLE_SPECULAR_TRACING",
                    CVarEnableSpecularTracing.Get());

                cmd.SetStencilTest(true);
                cmd.SetStencilCompareOp(vk::CompareOp::eNotEqual);
                cmd.SetStencilCompareMask(vk::StencilFaceFlagBits::eFrontAndBack, 1);
                cmd.SetStencilReference(vk::StencilFaceFlagBits::eFrontAndBack, 1);

                cmd.SetDepthTest(true, false);
                cmd.SetDepthCompareOp(vk::CompareOp::eLessOrEqual);

                cmd.SetBlending(true);
                cmd.SetBlendFunc(vk::BlendFactor::eOne,
                    vk::BlendFactor::eSrc1Color,
                    vk::BlendFactor::eZero,
                    vk::BlendFactor::eOne);

                cmd.SetImageView("shadowMap", "ShadowMap.Linear");
                cmd.SetImageView("voxelRadiance", "Voxels.Radiance");
                cmd.SetImageView("voxelNormals", "Voxels.Normals");

                cmd.SetUniformBuffer("VoxelStateUniform", "VoxelState");
                cmd.SetStorageBuffer("ExposureState", "ExposureState");
                cmd.SetUniformBuffer("ViewStates", "ViewState");
                cmd.SetUniformBuffer("LightData", "LightState");

                for (auto &voxelLayer : Voxels::VoxelLayers[voxelLayerCount - 1]) {
                    cmd.SetImageView(3, voxelLayer.dirIndex, resources.GetImageView(voxelLayer.fullName));
                }

                scene.DrawSceneIndirect(cmd,
                    resources.GetBuffer("WarpedVertexBuffer"),
                    resources.GetBuffer(drawIDs.drawCommandsBuffer),
                    resources.GetBuffer(drawIDs.drawParamsBuffer));
            });
    }
} // namespace sp::vulkan::renderer
