/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Async.hh"
#include "graphics/vulkan/core/Memory.hh"
#include "graphics/vulkan/core/VkCommon.hh"
#include "graphics/vulkan/scene/GPUScene.hh"

struct ImFontAtlas;
struct ImDrawData;

namespace sp {
    class GuiContext;
} // namespace sp

namespace sp::vulkan {
    struct VertexLayout;

    class GuiRenderer : public NonCopyable {
    public:
        GuiRenderer(DeviceContext &device, GPUScene &scene);
        void Render(GuiContext &context, CommandContext &cmd, vk::Rect2D viewport, glm::vec2 scale);
        void DrawGui(ImDrawData *drawData, CommandContext &cmd, vk::Rect2D viewport, glm::vec2 scale);
        void Tick();

    private:
        GPUScene &scene;
        double lastTime = 0.0, deltaTime;

        unique_ptr<VertexLayout> vertexLayout;

        shared_ptr<ImFontAtlas> fontAtlas;
        AsyncPtr<ImageView> fontView;
    };
} // namespace sp::vulkan
