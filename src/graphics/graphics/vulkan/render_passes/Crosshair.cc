/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Crosshair.hh"

#include "graphics/core/GraphicsContext.hh"
#include "graphics/vulkan/core/CommandContext.hh"

namespace sp::vulkan::renderer {
    static void drawDots(CommandContext &cmd, vk::Offset2D offset, glm::vec2 spread, glm::vec2 size) {
        vk::Rect2D viewport;
        viewport.extent.width = (uint32_t)glm::round(size.x);
        viewport.extent.height = (uint32_t)glm::round(size.y);
        viewport.offset = offset;

        cmd.SetViewport(viewport);
        cmd.Draw(3);

        viewport.offset.x = (int32_t)glm::round((float)offset.x + spread.x);
        cmd.SetViewport(viewport);
        cmd.Draw(3);

        viewport.offset.x = (int32_t)glm::round((float)offset.x - spread.x);
        cmd.SetViewport(viewport);
        cmd.Draw(3);

        viewport.offset.x = offset.x;
        viewport.offset.y = (int32_t)glm::round((float)offset.y + spread.y);
        cmd.SetViewport(viewport);
        cmd.Draw(3);

        viewport.offset.y = (int32_t)glm::round((float)offset.y - spread.y);
        cmd.SetViewport(viewport);
        cmd.Draw(3);
    }

    void AddCrosshair(RenderGraph &graph) {
        auto windowScale = CVarWindowScale.Get();
        graph.AddPass("Crosshair")
            .Build([&](PassBuilder &builder) {
                builder.SetColorAttachment(0, builder.LastOutputID(), {LoadOp::Load, StoreOp::Store});
            })
            .Execute([windowScale](Resources &resources, CommandContext &cmd) {
                cmd.SetDepthTest(false, false);
                cmd.SetShaders("screen_cover.vert", "solid_color.frag");

                auto extent = cmd.GetFramebufferExtent();
                auto center = vk::Offset2D{(int)extent.width / 2, (int)extent.height / 2};
                glm::vec2 spread = windowScale * (float)std::min(extent.width, extent.height) / 100.0f;
                glm::vec2 dotSize = glm::min(spread * 0.4f, windowScale * 2.0f);

                cmd.PushConstants(glm::vec4(1, 1, 0.95, 0.3));
                cmd.SetBlendFuncSeparate(vk::BlendFactor::eSrcAlpha,
                    vk::BlendFactor::eOne,
                    vk::BlendFactor::eZero,
                    vk::BlendFactor::eOne);
                cmd.SetBlending(true, vk::BlendOp::eAdd);
                drawDots(cmd, center, spread, dotSize);

                cmd.PushConstants(glm::vec4(0.6, 0.6, 0.5, 1));
                cmd.SetBlendFuncSeparate(vk::BlendFactor::eSrcAlpha,
                    vk::BlendFactor::eZero,
                    vk::BlendFactor::eZero,
                    vk::BlendFactor::eOne);
                cmd.SetBlending(true, vk::BlendOp::eMin);
                drawDots(cmd, center, spread, dotSize);
            });
    }
} // namespace sp::vulkan::renderer
