#pragma once

#include "assets/Async.hh"
#include "console/CFunc.hh"
#include "ecs/Ecs.hh"
#include "graphics/core/RenderTarget.hh"
#include "graphics/vulkan/core/Common.hh"
#include "graphics/vulkan/core/Memory.hh"
#include "graphics/vulkan/render_graph/RenderGraph.hh"
#include "graphics/vulkan/render_passes/Emissive.hh"
#include "graphics/vulkan/render_passes/Lighting.hh"
#include "graphics/vulkan/render_passes/SMAA.hh"
#include "graphics/vulkan/render_passes/Screenshots.hh"
#include "graphics/vulkan/render_passes/Voxels.hh"
#include "graphics/vulkan/scene/GPUScene.hh"

#include <atomic>
#include <functional>
#include <robin_hood.h>

namespace sp {
    class GuiManager;

#ifdef SP_XR_SUPPORT
    namespace xr {
        class XrSystem;
    }
#endif
} // namespace sp

namespace sp::vulkan {
    class Mesh;
    class GuiRenderer;

    extern CVar<string> CVarWindowViewTarget;

    class Renderer {
    public:
        Renderer(DeviceContext &context);
        ~Renderer();

        void RenderFrame();
        void EndFrame();

        void SetDebugGui(GuiManager &gui);

#ifdef SP_XR_SUPPORT
        void SetXRSystem(shared_ptr<xr::XrSystem> xr) {
            xrSystem = xr;
        }
#endif

    private:
        DeviceContext &device;
        rg::RenderGraph graph;

        void BuildFrameGraph();
        void AddFlatView(ecs::Lock<ecs::Read<ecs::TransformSnapshot, ecs::View>> lock);
        void AddWindowOutput();

#ifdef SP_XR_SUPPORT
        void AddXRView(ecs::Lock<ecs::Read<ecs::TransformSnapshot, ecs::View, ecs::XRView>> lock);
        void AddXRSubmit(ecs::Lock<ecs::Read<ecs::XRView>> lock);
#endif

        void AddGuis(ecs::Lock<ecs::Read<ecs::Gui>> lock);
        void AddDeferredPasses(ecs::Lock<ecs::Read<ecs::TransformSnapshot, ecs::Screen, ecs::LaserLine>> lock);
        void AddMenuOverlay();

        CFuncCollection funcs;

        GPUScene scene;

        renderer::Lighting lighting;
        renderer::Voxels voxels;
        renderer::Emissive emissive;
        renderer::SMAA smaa;
        renderer::Screenshots screenshots;

        unique_ptr<GuiRenderer> guiRenderer;
        struct RenderableGui {
            ecs::Entity entity;
            GuiManager *manager;
            rg::ResourceID renderGraphID = rg::InvalidResource;
        };
        vector<RenderableGui> guis;
        GuiManager *debugGui = nullptr;

        ecs::ComponentObserver<ecs::Gui> guiObserver;

        bool listRenderTargets = false;

#ifdef SP_XR_SUPPORT
        shared_ptr<xr::XrSystem> xrSystem;
        std::vector<glm::mat4> xrRenderPoses;
        std::array<BufferPtr, 2> hiddenAreaMesh;
        std::array<uint32, 2> hiddenAreaTriangleCount;
#endif
    };
} // namespace sp::vulkan
