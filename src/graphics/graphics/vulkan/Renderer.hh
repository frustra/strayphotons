#pragma once

#include "core/CFunc.hh"
#include "core/PreservingMap.hh"
#include "ecs/Ecs.hh"
#include "graphics/core/RenderTarget.hh"
#include "graphics/vulkan/GPUTypes.hh"
#include "graphics/vulkan/core/Common.hh"
#include "graphics/vulkan/core/Memory.hh"

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
    class Model;
    class GuiRenderer;
    class RenderGraph;

    struct GPUViewState {
        GPUViewState() {}
        GPUViewState(const ecs::View &view) {
            projMat = view.projMat;
            invProjMat = view.invProjMat;
            viewMat = view.viewMat;
            invViewMat = view.invViewMat;
            clip = view.clip;
            extents = view.extents;
        }

        glm::mat4 projMat, invProjMat;
        glm::mat4 viewMat, invViewMat;
        glm::vec2 clip, extents;
    };
    static_assert(sizeof(GPUViewState) % 16 == 0, "std140 alignment");

    struct LightingContext {
        int count = 0;
        glm::ivec2 renderTargetSize = {};
        ecs::View views[MAX_LIGHTS];

        struct GPUData {
            GPULight lights[MAX_LIGHTS];
            int count;
            float padding[3];
        } gpuData;
    };

    class Renderer {
    public:
        using DrawLock = typename ecs::Lock<ecs::Read<ecs::Renderable, ecs::Light, ecs::Transform>>;
        typedef std::function<void(DrawLock, Tecs::Entity &)> PreDrawFunc;

        Renderer(DeviceContext &context);
        ~Renderer();

        void RenderFrame();

        void RenderShadowMaps(RenderGraph &graph, DrawLock lock);

        void ForwardPass(CommandContext &cmd,
                         ecs::Renderable::VisibilityMask viewMask,
                         DrawLock lock,
                         bool useMaterial = true,
                         const PreDrawFunc &preDraw = {});

        void DrawEntity(CommandContext &cmd,
                        ecs::Renderable::VisibilityMask viewMask,
                        DrawLock lock,
                        Tecs::Entity &ent,
                        bool useMaterial = true,
                        const PreDrawFunc &preDraw = {});

        float Exposure = 1.0f;

        DeviceContext &device;

        void SetDebugGui(GuiManager &gui);

#ifdef SP_XR_SUPPORT
        void SetXRSystem(xr::XrSystem *xr) {
            xrSystem = xr;
        }
#endif

        void QueueScreenshot(const string &path, const string &resource);

    private:
        void AddScreenshotPasses(RenderGraph &graph);
        RenderGraphResourceID VisualizeBuffer(RenderGraph &graph, RenderGraphResourceID sourceID);
        void LoadLightState(ecs::Lock<ecs::Read<ecs::Light, ecs::Transform>> lock);
        void EndFrame();

        CFuncCollection funcs;
        PreservingMap<string, Model> activeModels;
        unique_ptr<GuiRenderer> debugGuiRenderer;

        vector<std::pair<string, string>> pendingScreenshots;
        bool listRenderTargets = false;

        LightingContext lights;

#ifdef SP_XR_SUPPORT
        xr::XrSystem *xrSystem = nullptr;
        std::vector<glm::mat4> xrRenderPoses;
#endif
    };
} // namespace sp::vulkan
