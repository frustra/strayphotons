#pragma once

#include "core/CFunc.hh"
#include "core/PreservingMap.hh"
#include "ecs/Ecs.hh"
#include "ecs/components/Renderable.hh"
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

    struct ViewStateUniforms {
        glm::mat4 view[2];
        glm::mat4 projection[2];
        glm::vec2 clip[2];
    };

    struct LightingContext {
        int count = 0;
        glm::ivec2 renderTargetSize = {};
        GPULight gpuData[MAX_LIGHTS];
        ecs::View views[MAX_LIGHTS];
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
                         const PreDrawFunc &preDraw = {});

        void DrawEntity(CommandContext &cmd,
                        ecs::Renderable::VisibilityMask viewMask,
                        DrawLock lock,
                        Tecs::Entity &ent,
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
        void VisualizeBuffer(RenderGraph &graph, string_view name);
        void LoadLightState(ecs::Lock<ecs::Read<ecs::Light, ecs::Transform>> lock);
        void EndFrame();

        CFuncCollection funcs;
        PreservingMap<string, Model> activeModels;
        unique_ptr<GuiRenderer> debugGuiRenderer;

        vector<std::pair<string, string>> pendingScreenshots;

        LightingContext lights;

#ifdef SP_XR_SUPPORT
        xr::XrSystem *xrSystem = nullptr;
        std::vector<glm::mat4> xrRenderPoses;
#endif
    };
} // namespace sp::vulkan
