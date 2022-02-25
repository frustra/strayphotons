#pragma once

#include "assets/Async.hh"
#include "console/CFunc.hh"
#include "core/LockFreeMutex.hh"
#include "core/PreservingMap.hh"
#include "ecs/Ecs.hh"
#include "graphics/core/RenderTarget.hh"
#include "graphics/vulkan/GPUSceneContext.hh"
#include "graphics/vulkan/GPUTypes.hh"
#include "graphics/vulkan/core/Common.hh"
#include "graphics/vulkan/core/Memory.hh"
#include "graphics/vulkan/render_graph/RenderGraph.hh"
#include "graphics/vulkan/render_passes/Emissive.hh"
#include "graphics/vulkan/render_passes/SMAA.hh"

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
        using DrawLock = typename ecs::Lock<ecs::Read<ecs::Renderable, ecs::Light, ecs::TransformSnapshot>>;
        typedef std::function<void(DrawLock, ecs::Entity &)> PreDrawFunc;

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

        void QueueScreenshot(const string &path, const string &resource);

        ImageViewPtr GetBlankPixelImage();
        ImageViewPtr CreateSinglePixelImage(glm::vec4 value);

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

        void AddScreenshots();
        rg::ResourceID VisualizeBuffer(rg::ResourceID sourceID, uint32 arrayLayer = ~0u);

        void AddSceneState(ecs::Lock<ecs::Read<ecs::Renderable, ecs::TransformSnapshot>> lock);
        void AddGeometryWarp();
        void AddLightState(ecs::Lock<ecs::Read<ecs::Light, ecs::TransformSnapshot>> lock);
        void AddShadowMaps(DrawLock lock);
        void AddGuis(ecs::Lock<ecs::Read<ecs::Gui>> lock);
        void AddDeferredPasses(ecs::Lock<ecs::Read<ecs::TransformSnapshot, ecs::Screen, ecs::LaserLine>> lock);
        void AddLighting();
        void AddTonemap();
        void AddMenuOverlay();

        rg::ResourceID AddBloom();
        rg::ResourceID AddGaussianBlur(rg::ResourceID sourceID,
            glm::ivec2 direction,
            uint32 downsample = 1,
            float scale = 1.0f,
            float clip = FLT_MAX);

        struct DrawBufferIDs {
            rg::ResourceID drawCommandsBuffer; // first 4 bytes are the number of draws
            rg::ResourceID drawParamsBuffer;
        };

        DrawBufferIDs GenerateDrawsForView(ecs::Renderable::VisibilityMask viewMask);
        void DrawSceneIndirect(CommandContext &cmd,
            rg::Resources &resources,
            BufferPtr drawCommandsBuffer,
            BufferPtr drawParamsBuffer);

        CFuncCollection funcs;

        LightingContext lights;
        GPUSceneContext scene;
        PreservingMap<string, Mesh> activeMeshes;
        vector<std::pair<std::shared_ptr<const sp::Gltf>, size_t>> meshesToLoad;

        unique_ptr<GuiRenderer> guiRenderer;
        struct RenderableGui {
            ecs::Entity entity;
            GuiManager *manager;
            rg::ResourceID renderGraphID = rg::InvalidResource;
        };
        vector<RenderableGui> guis;
        GuiManager *debugGui = nullptr;

        ecs::ComponentObserver<ecs::Gui> guiObserver;

        LockFreeMutex screenshotMutex;
        vector<std::pair<string, string>> pendingScreenshots;
        bool listRenderTargets = false;

        renderer::Emissive emissive;
        renderer::SMAA smaa;

        struct EmptyImageKey {
            vk::Format format;
        };

        ImageViewPtr blankPixelImage;

        PreservingMap<ImageView *, ImageView> debugViews;

        std::atomic_flag sceneReady, pendingTransaction;

#ifdef SP_XR_SUPPORT
        shared_ptr<xr::XrSystem> xrSystem;
        std::vector<glm::mat4> xrRenderPoses;
        std::array<BufferPtr, 2> hiddenAreaMesh;
        std::array<uint32, 2> hiddenAreaTriangleCount;
#endif
    };
} // namespace sp::vulkan
