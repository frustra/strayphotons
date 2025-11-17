/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Async.hh"
#include "console/CFunc.hh"
#include "ecs/Ecs.hh"
#include "graphics/vulkan/core/Memory.hh"
#include "graphics/vulkan/core/VkCommon.hh"
#include "graphics/vulkan/render_graph/RenderGraph.hh"
#include "graphics/vulkan/render_passes/Emissive.hh"
#include "graphics/vulkan/render_passes/Lighting.hh"
#include "graphics/vulkan/render_passes/SMAA.hh"
#include "graphics/vulkan/render_passes/Screenshots.hh"
#include "graphics/vulkan/render_passes/Transparency.hh"
#include "graphics/vulkan/render_passes/Voxels.hh"
#include "graphics/vulkan/scene/GPUScene.hh"

#include <atomic>
#include <functional>
#include <robin_hood.h>

namespace sp {
    class GuiContext;
    class Game;
} // namespace sp

namespace sp::xr {
    class XrSystem;
}

namespace sp::vulkan {
    class Mesh;
    class GuiRenderer;

    extern CVar<string> CVarWindowViewTarget;

    class Renderer {
    public:
        Renderer(Game &game, DeviceContext &context);
        ~Renderer();

        void RenderFrame(chrono_clock::duration elapsedTime);
        void EndFrame();

        GuiRenderer *GetGuiRenderer() const;

    private:
        Game &game;
        DeviceContext &device;
        rg::RenderGraph graph;

        void BuildFrameGraph(chrono_clock::duration elapsedTime);
        ecs::View AddFlatView(ecs::Lock<ecs::Read<ecs::TransformSnapshot, ecs::View>> lock, ecs::Entity ent);
        void AddWindowOutput();

        ecs::View AddXrView(ecs::Lock<ecs::Read<ecs::TransformSnapshot, ecs::View, ecs::XrView>> lock);
        void AddXrSubmit(ecs::Lock<ecs::Read<ecs::XrView>> lock);

        void UpdateRenderOutputs(ecs::Lock<ecs::Read<ecs::Name>, ecs::Write<ecs::RenderOutput>> lock);
        void AddOutputPasses(
            ecs::Lock<ecs::Read<ecs::TransformSnapshot, ecs::View, ecs::Screen, ecs::LaserLine, ecs::GuiElement>> lock,
            chrono_clock::duration elapsedTime);
        void AddDeferredPasses(ecs::Lock<ecs::Read<ecs::TransformSnapshot, ecs::Screen, ecs::LaserLine>> lock,
            const ecs::View &view,
            chrono_clock::duration elapsedTime);

        CFuncCollection funcs;
        vk::Format depthStencilFormat;

        GPUScene scene;

        renderer::Voxels voxels;
        renderer::Lighting lighting;
        renderer::Transparency transparency;
        renderer::Emissive emissive;
        renderer::SMAA smaa;
        renderer::Screenshots screenshots;

        unique_ptr<GuiRenderer> guiRenderer;
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
        GuiContext *overlayGui = nullptr, *menuGui = nullptr;
        AsyncPtr<ImageView> logoTex;

        ecs::ComponentModifiedObserver<ecs::RenderOutput> renderOutputObserver;
        ecs::ComponentModifiedObserver<ecs::Renderable> renderableObserver;
        ecs::ComponentModifiedObserver<ecs::Light> lightObserver;

        bool listImages = false;

        std::vector<glm::mat4> xrRenderPoses;
        std::array<BufferPtr, 2> hiddenAreaMesh;
        std::array<uint32, 2> hiddenAreaTriangleCount;
    };
} // namespace sp::vulkan
