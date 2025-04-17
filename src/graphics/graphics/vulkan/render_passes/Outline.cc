/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Outline.hh"

#include "Readback.hh"
#include "console/CVar.hh"
#include "ecs/EcsImpl.hh"
#include "graphics/vulkan/core/CommandContext.hh"

namespace sp::vulkan::renderer {
    CVar<bool> CVarDisableOutlineStencil("r.DisableOutlineStencil", false, "Disable the outline stencil render pass");

    void AddOutlines(RenderGraph &graph, GPUScene &scene) {
        if (CVarDisableOutlineStencil.Get()) return;

        auto drawIDs = scene.GenerateDrawsForView(graph, ecs::VisibilityMask::OutlineSelection);

        graph.AddPass("OutlinesStencil")
            .Build([&](PassBuilder &builder) {
                builder.ReadUniform("ViewState");
                builder.Read("WarpedVertexBuffer", Access::VertexBuffer);
                builder.Read(drawIDs.drawCommandsBuffer, Access::IndirectBuffer);
                builder.Read(drawIDs.drawParamsBuffer, Access::VertexShaderReadStorage);

                builder.SetDepthAttachment("GBufferDepthStencil", {LoadOp::Load, StoreOp::Store});
            })
            .Execute([drawIDs, scene = &scene](Resources &resources, CommandContext &cmd) {
                cmd.SetShaders("scene.vert", "solid_color.frag");
                cmd.PushConstants(glm::vec4(1, 1, 0.5, 0.2));
                cmd.SetUniformBuffer("ViewStates", "ViewState");
                cmd.SetDepthTest(false, false);
                cmd.SetDepthCompareOp(vk::CompareOp::eLessOrEqual);

                cmd.SetStencilTest(true);
                cmd.SetStencilWriteMask(vk::StencilFaceFlagBits::eFrontAndBack, 2);
                cmd.SetStencilReference(vk::StencilFaceFlagBits::eFrontAndBack, 2);
                cmd.SetStencilCompareOp(vk::CompareOp::eAlways);
                cmd.SetStencilPassOp(vk::StencilOp::eReplace);
                cmd.SetStencilFailOp(vk::StencilOp::eReplace);
                cmd.SetStencilDepthFailOp(vk::StencilOp::eKeep);

                scene->DrawSceneIndirect(cmd,
                    resources.GetBuffer("WarpedVertexBuffer"),
                    resources.GetBuffer(drawIDs.drawCommandsBuffer),
                    resources.GetBuffer(drawIDs.drawParamsBuffer));
            });

        graph.AddPass("Outlines")
            .Build([&](PassBuilder &builder) {
                builder.ReadUniform("ViewState");
                builder.Read("WarpedVertexBuffer", Access::VertexBuffer);
                builder.Read(drawIDs.drawCommandsBuffer, Access::IndirectBuffer);
                builder.Read(drawIDs.drawParamsBuffer, Access::VertexShaderReadStorage);

                builder.SetColorAttachment(0, builder.LastOutputID(), {LoadOp::Load, StoreOp::Store});
                builder.SetDepthAttachment("GBufferDepthStencil", {LoadOp::Load, StoreOp::ReadOnly});
            })
            .Execute([drawIDs, scene = &scene](Resources &resources, CommandContext &cmd) {
                cmd.SetShaders("scene.vert", "solid_color.frag");
                cmd.PushConstants(glm::vec4(glm::vec3(4, 10, 0.5), 1));
                cmd.SetUniformBuffer("ViewStates", "ViewState");
                cmd.SetDepthTest(false, false);
                cmd.SetDepthCompareOp(vk::CompareOp::eLessOrEqual);

                cmd.SetStencilTest(true);
                cmd.SetStencilCompareOp(vk::CompareOp::eEqual);
                cmd.SetStencilCompareMask(vk::StencilFaceFlagBits::eFrontAndBack, 3);
                cmd.SetStencilReference(vk::StencilFaceFlagBits::eFrontAndBack, 0);
                cmd.SetBlending(true);
                cmd.SetPolygonMode(vk::PolygonMode::eLine);
                cmd.SetLineWidth(4.0f);

                scene->DrawSceneIndirect(cmd,
                    resources.GetBuffer("WarpedVertexBuffer"),
                    resources.GetBuffer(drawIDs.drawCommandsBuffer),
                    resources.GetBuffer(drawIDs.drawParamsBuffer));
            });
    }
} // namespace sp::vulkan::renderer
