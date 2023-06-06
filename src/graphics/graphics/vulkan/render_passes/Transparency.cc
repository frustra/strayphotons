/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Transparency.hh"

#include "ecs/components/View.hh"
#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/core/PerfTimer.hh"

namespace sp::vulkan::renderer {
    void Transparency::AddPass(RenderGraph &graph, const ecs::View &view) {
        glm::vec3 viewPos = view.invViewMat * glm::vec4(0, 0, 0, 1);
        auto drawIDs = scene.GenerateSortedDrawsForView(graph, viewPos, ecs::VisibilityMask::Transparent, true);

        graph.AddPass("Transparency")
            .Build([&](PassBuilder &builder) {
                builder.Read("ExposureState", Access::FragmentShaderReadStorage);
                builder.Read("ShadowMap.Linear", Access::FragmentShaderSampleImage);
                builder.Read("Voxels.Radiance", Access::FragmentShaderSampleImage);
                builder.Read("Voxels.Normals", Access::FragmentShaderSampleImage);
                builder.ReadUniform("ViewState");
                builder.ReadUniform("LightState");
                builder.ReadUniform("VoxelState");

                builder.Read("WarpedVertexBuffer", Access::VertexBuffer);
                builder.Read(drawIDs.drawCommandsBuffer, Access::IndirectBuffer);
                builder.Read(drawIDs.drawParamsBuffer, Access::VertexShaderReadStorage);

                builder.SetColorAttachment(0, builder.LastOutputID(), {LoadOp::Load, StoreOp::Store});
                builder.SetDepthAttachment("GBufferDepthStencil", {LoadOp::Load, StoreOp::ReadOnly});
            })
            .Execute([this, drawIDs](Resources &resources, CommandContext &cmd) {
                cmd.SetShaders("scene.vert", "lighting_transparent.frag");

                cmd.SetStencilTest(true);
                cmd.SetStencilCompareOp(vk::CompareOp::eNotEqual);
                cmd.SetStencilCompareMask(vk::StencilFaceFlagBits::eFrontAndBack, 1);
                cmd.SetStencilReference(vk::StencilFaceFlagBits::eFrontAndBack, 1);

                cmd.SetDepthTest(true, false);
                cmd.SetDepthCompareOp(vk::CompareOp::eLessOrEqual);

                cmd.SetBlending(true);
                cmd.SetBlendFuncSeparate(vk::BlendFactor::eOne,
                    vk::BlendFactor::eSrc1Color,
                    vk::BlendFactor::eZero,
                    vk::BlendFactor::eOne);

                cmd.SetImageView(0, 0, resources.GetImageView("ShadowMap.Linear"));
                cmd.SetImageView(0, 1, resources.GetImageView("Voxels.Radiance"));
                cmd.SetImageView(0, 2, resources.GetImageView("Voxels.Normals"));

                cmd.SetUniformBuffer(0, 8, resources.GetBuffer("VoxelState"));
                cmd.SetStorageBuffer(0, 9, resources.GetBuffer("ExposureState"));
                cmd.SetUniformBuffer(0, 10, resources.GetBuffer("ViewState"));
                cmd.SetUniformBuffer(0, 11, resources.GetBuffer("LightState"));

                scene.DrawSceneIndirect(cmd,
                    resources.GetBuffer("WarpedVertexBuffer"),
                    resources.GetBuffer(drawIDs.drawCommandsBuffer),
                    resources.GetBuffer(drawIDs.drawParamsBuffer));
            });
    }
} // namespace sp::vulkan::renderer
