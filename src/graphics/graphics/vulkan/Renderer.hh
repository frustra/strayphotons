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

        void SetOverlayGui(GuiContext *gui);
        void SetMenuGui(GuiContext *gui);

    private:
        Game &game;
        DeviceContext &device;
        rg::RenderGraph graph;

        void BuildFrameGraph(chrono_clock::duration elapsedTime);
        ecs::View AddFlatView(ecs::Lock<ecs::Read<ecs::TransformSnapshot, ecs::View>> lock);
        void AddWindowOutput();

        ecs::View AddXrView(ecs::Lock<ecs::Read<ecs::TransformSnapshot, ecs::View, ecs::XrView>> lock);
        void AddXrSubmit(ecs::Lock<ecs::Read<ecs::XrView>> lock);

        void AddGui(ecs::Entity ent, const ecs::Gui &gui, const ecs::Scripts *scripts);
        void AddWorldGuis(
            ecs::Lock<ecs::Read<ecs::TransformSnapshot, ecs::Gui, ecs::Screen, ecs::Name, ecs::Scripts>> lock);
        void AddMenuGui(ecs::Lock<ecs::Read<ecs::View>> lock);
        void AddDeferredPasses(ecs::Lock<ecs::Read<ecs::TransformSnapshot, ecs::Screen, ecs::Gui, ecs::LaserLine>> lock,
            const ecs::View &view,
            chrono_clock::duration elapsedTime);
        void AddMenuOverlay();

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
        struct RenderableGui {
            ecs::Entity entity;
            glm::vec2 scale;
            GuiContext *context;
            shared_ptr<GuiContext> contextShared;
            rg::ResourceID renderGraphID = rg::InvalidResource;
        };
        vector<RenderableGui> guis;
        GuiContext *overlayGui = nullptr, *menuGui = nullptr;
        AsyncPtr<ImageView> logoTex;

        ecs::ComponentAddRemoveObserver<ecs::Gui> guiObserver;
        ecs::ComponentModifiedObserver<ecs::Renderable> renderableObserver;
        ecs::ComponentModifiedObserver<ecs::Light> lightObserver;

        bool listImages = false;

        std::vector<glm::mat4> xrRenderPoses;
        std::array<BufferPtr, 2> hiddenAreaMesh;
        std::array<uint32, 2> hiddenAreaTriangleCount;
    };
} // namespace sp::vulkan
