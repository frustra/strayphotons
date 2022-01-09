#pragma once

#include "console/CFunc.hh"
#include "core/PreservingMap.hh"
#include "ecs/Ecs.hh"
#include "graphics/core/RenderTarget.hh"
#include "graphics/vulkan/GPUTypes.hh"
#include "graphics/vulkan/SceneMeshContext.hh"
#include "graphics/vulkan/core/Common.hh"
#include "graphics/vulkan/core/Memory.hh"
#include "graphics/vulkan/core/PerfTimer.hh"

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

        int gelCount = 0;
        string gelNames[MAX_LIGHT_GELS];

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

        Renderer(DeviceContext &context, PerfTimer &timer);
        ~Renderer();

        void RenderFrame();

        struct DrawBufferIDs {
            RenderGraphResourceID drawCommandsBuffer; // first 4 bytes are the number of draws
            RenderGraphResourceID drawParamsBuffer;
        };

        DrawBufferIDs GenerateDrawsForView(RenderGraph &graph, ecs::Renderable::VisibilityMask viewMask);
        void ForwardPass3(CommandContext &cmd, BufferPtr drawCommandsBuffer, BufferPtr drawParamsBuffer);

        void ForwardPass2(CommandContext &cmd, ecs::Renderable::VisibilityMask viewMask);

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

        void SetDebugGui(GuiManager &gui);

#ifdef SP_XR_SUPPORT
        void SetXRSystem(xr::XrSystem *xr) {
            xrSystem = xr;
        }
#endif

        void QueueScreenshot(const string &path, const string &resource);

        ImageViewPtr GetBlankPixelImage();
        ImageViewPtr CreateSinglePixelImage(glm::vec4 value);

    private:
        DeviceContext &device;
        PerfTimer &timer;

        void EndFrame();
        void BuildFrameGraph(RenderGraph &graph);

        void AddScreenshotPasses(RenderGraph &graph);
        RenderGraphResourceID VisualizeBuffer(RenderGraph &graph,
            RenderGraphResourceID sourceID,
            uint32 arrayLayer = ~0u);

        void AddSceneState(RenderGraph &graph, ecs::Lock<ecs::Read<ecs::Renderable, ecs::Transform>> lock);
        void AddLightState(RenderGraph &graph, ecs::Lock<ecs::Read<ecs::Light, ecs::Transform>> lock);
        void AddShadowMaps(RenderGraph &graph, DrawLock lock);
        void AddGuis(RenderGraph &graph, ecs::Lock<ecs::Read<ecs::Gui>> lock);
        void AddDeferredPasses(RenderGraph &graph);
        void AddMenuOverlay(RenderGraph &graph);

        RenderGraphResourceID AddBloom(RenderGraph &graph, RenderGraphResourceID sourceID);
        RenderGraphResourceID AddGaussianBlur(RenderGraph &graph,
            RenderGraphResourceID sourceID,
            glm::ivec2 direction,
            uint32 downsample = 1,
            float scale = 1.0f,
            float clip = FLT_MAX);

        CFuncCollection funcs;

        LightingContext lights;
        SceneMeshContext scene;
        PreservingMap<string, Model> activeModels;

        struct RenderableGui {
            Tecs::Entity entity;
            shared_ptr<GuiRenderer> renderer;
            float luminanceScale;
            RenderGraphResourceID renderGraphID = ~0u;
        };
        vector<RenderableGui> guis;
        unique_ptr<GuiRenderer> debugGuiRenderer;

        ecs::ComponentObserver<ecs::Gui> guiObserver;

        vector<std::pair<string, string>> pendingScreenshots;
        bool listRenderTargets = false;

        struct EmptyImageKey {
            vk::Format format;
        };

        ImageViewPtr blankPixelImage;

        PreservingMap<ImageView *, ImageView> debugViews;

#ifdef SP_XR_SUPPORT
        xr::XrSystem *xrSystem = nullptr;
        std::vector<glm::mat4> xrRenderPoses;
        std::array<BufferPtr, 2> hiddenAreaMesh;
        std::array<uint32, 2> hiddenAreaTriangleCount;
#endif
    };
} // namespace sp::vulkan
