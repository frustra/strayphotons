/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Async.hh"
#include "graphics/GenericCompositor.hh"
#include "graphics/vulkan/core/Memory.hh"
#include "graphics/vulkan/core/VkCommon.hh"
#include "graphics/vulkan/scene/GPUScene.hh"

struct ImFontAtlas;
struct ImDrawData;

namespace sp {
    class GuiContext;
} // namespace sp

namespace sp::vulkan {
    class Renderer;
    struct VertexLayout;

    class Compositor : public GenericCompositor {
    public:
        Compositor(DeviceContext &device, Renderer &renderer);

        void DrawGui(ImDrawData *drawData, glm::ivec4 viewport, glm::vec2 scale) override;

        void BeforeFrame();
        void UpdateRenderOutputs(ecs::Lock<ecs::Read<ecs::Name>, ecs::Write<ecs::RenderOutput>> lock);
        void AddOutputPasses();
        void EndFrame();

    private:
        void internalDrawGui(ImDrawData *drawData, vk::Rect2D viewport, glm::vec2 scale, bool allowUserCallback);

        Renderer &renderer;
        GPUScene &scene;
        rg::RenderGraph &graph;
        double lastTime = 0.0, deltaTime;

        struct RenderOutputInfo {
            ecs::Name entityName;
            ecs::Entity entity;
            glm::ivec2 outputSize;
            glm::vec2 scale;
            std::shared_ptr<GuiContext> guiContext;
            bool enableGui;
            std::vector<ecs::EntityRef> guiElements;
            rg::ResourceName sourceName;
            std::variant<rg::ResourceID, TextureIndex> sourceTexture = rg::InvalidResource;
            rg::ResourceID renderGraphID = rg::InvalidResource;
        };
        vector<RenderOutputInfo> renderOutputs;

        unique_ptr<VertexLayout> vertexLayout;

        shared_ptr<ImFontAtlas> fontAtlas;
        AsyncPtr<ImageView> fontView;
    };
} // namespace sp::vulkan
