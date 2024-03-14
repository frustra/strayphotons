/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Crosshair.hh"

#include "graphics/vulkan/core/CommandContext.hh"

namespace sp::vulkan::renderer {
    static void drawDots(CommandContext &cmd, vk::Offset2D offset, uint32 spread, uint32 size) {
        vk::Rect2D viewport;
        viewport.extent.width = size;
        viewport.extent.height = size;
        viewport.offset = offset;

        cmd.SetViewport(viewport);
        cmd.Draw(3);

        viewport.offset.x = offset.x + spread;
        cmd.SetViewport(viewport);
        cmd.Draw(3);

        viewport.offset.x = offset.x - spread;
        cmd.SetViewport(viewport);
        cmd.Draw(3);

        viewport.offset.x = offset.x;
        viewport.offset.y = offset.y + spread;
        cmd.SetViewport(viewport);
        cmd.Draw(3);

        viewport.offset.y = offset.y - spread;
        cmd.SetViewport(viewport);
        cmd.Draw(3);
    }

    void AddCrosshair(RenderGraph &graph) {
        graph.AddPass("Crosshair")
            .Build([&](PassBuilder &builder) {
                builder.SetColorAttachment(0, builder.LastOutputID(), {LoadOp::Load, StoreOp::Store});
            })
            .Execute([](Resources &resources, CommandContext &cmd) {
                cmd.SetDepthTest(false, false);
                cmd.SetShaders("screen_cover.vert", "solid_color.frag");

                auto extent = cmd.GetFramebufferExtent();
                auto center = vk::Offset2D{(int)extent.width / 2, (int)extent.height / 2};
                auto spread = extent.width / 100;

                cmd.PushConstants(glm::vec4(1, 1, 0.95, 0.3));
                cmd.SetBlendFuncSeparate(vk::BlendFactor::eSrcAlpha,
                    vk::BlendFactor::eOne,
                    vk::BlendFactor::eZero,
                    vk::BlendFactor::eOne);
                cmd.SetBlending(true, vk::BlendOp::eAdd);
                drawDots(cmd, center, spread, 2);

                cmd.PushConstants(glm::vec4(0.6, 0.6, 0.5, 1));
                cmd.SetBlendFuncSeparate(vk::BlendFactor::eSrcAlpha,
                    vk::BlendFactor::eZero,
                    vk::BlendFactor::eZero,
                    vk::BlendFactor::eOne);
                cmd.SetBlending(true, vk::BlendOp::eMin);
                drawDots(cmd, center, spread, 2);
            });
    }
} // namespace sp::vulkan::renderer
