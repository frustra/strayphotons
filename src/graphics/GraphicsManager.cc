#ifdef SP_GRAPHICS_SUPPORT
    #include "GraphicsManager.hh"

    #include "console/CVar.hh"
    #include "core/Logging.hh"
    #include "core/Tracing.hh"
    #include "ecs/EcsImpl.hh"
    #include "ecs/EntityReferenceManager.hh"
    #include "graphics/core/GraphicsContext.hh"
    #include "main/Game.hh"

    #ifdef SP_GRAPHICS_SUPPORT_GL
        #include "graphics/opengl/GlfwGraphicsContext.hh"
        #include "graphics/opengl/RenderTargetPool.hh"
        #include "graphics/opengl/gui/ProfilerGui.hh"
        #include "graphics/opengl/voxel_renderer/VoxelRenderer.hh"
        #include "input/glfw/GlfwInputHandler.hh"
    #endif
    #ifdef SP_GRAPHICS_SUPPORT_VK
        #include "graphics/vulkan/Renderer.hh"
        #include "graphics/vulkan/core/DeviceContext.hh"
        #include "graphics/vulkan/gui/ProfilerGui.hh"
    #endif

    #include <algorithm>
    #include <cxxopts.hpp>
    #include <glm/gtc/matrix_access.hpp>
    #include <iostream>
    #include <optional>
    #include <system_error>
    #include <thread>

namespace sp {
    static CVar<std::string> CVarFlatviewEntity("r.FlatviewEntity",
        "player:flatview",
        "The entity with a View component to display");

    #ifdef SP_TEST_MODE
    static std::atomic_uint64_t stepCount, maxStepCount;

    static CFunc<int> CFuncStepGraphics("stepgraphics",
        "Renders N frames in a row, saving any queued screenshots, default is 1",
        [](int arg) {
            maxStepCount += std::max(1, arg);
            auto step = stepCount.load();
            while (step < maxStepCount) {
                stepCount.wait(step);
                step = stepCount.load();
            }
        });
    #endif

    #if defined(SP_GRAPHICS_SUPPORT_HEADLESS) || defined(SP_TEST_MODE)
    const uint32 DefaultMaxFPS = 90;
    #elif defined(SP_DEBUG)
    const uint32 DefaultMaxFPS = 144;
    #else
    const uint32 DefaultMaxFPS = 0;
    #endif

    static CVar<uint32> CVarMaxFPS("r.MaxFPS",
        DefaultMaxFPS,
        "wait between frames to target this framerate (0 to disable)");

    GraphicsManager::GraphicsManager(Game *game) : game(game) {}

    GraphicsManager::~GraphicsManager() {
        if (context) context->WaitIdle();
    }

    void GraphicsManager::Init() {
        ZoneScoped;
        Assert(!context, "already have a graphics context");

    #ifdef SP_GRAPHICS_SUPPORT_GL
        Logf("Graphics starting up (OpenGL)");
    #endif
    #ifdef SP_GRAPHICS_SUPPORT_VK
        Logf("Graphics starting up (Vulkan)");
    #endif

    #ifdef SP_GRAPHICS_SUPPORT_GL
        auto glfwContext = new GlfwGraphicsContext();
        context.reset(glfwContext);

        GLFWwindow *window = glfwContext->GetWindow();
    #endif

    #ifdef SP_GRAPHICS_SUPPORT_VK
        bool enableValidationLayers = game->options.count("with-validation-layers") > 0;

        #ifdef SP_GRAPHICS_SUPPORT_HEADLESS
        bool enableSwapchain = false;
        #else
        bool enableSwapchain = true;
        #endif
        auto vkContext = new vulkan::DeviceContext(enableValidationLayers, enableSwapchain);
        context.reset(vkContext);

        GLFWwindow *window = vkContext->GetWindow();
    #endif

        if (window != nullptr) glfwInputHandler = make_unique<GlfwInputHandler>(*window);

        if (game->options.count("size")) {
            std::istringstream ss(game->options["size"].as<string>());
            glm::ivec2 size;
            ss >> size.x >> size.y;

            if (size.x > 0 && size.y > 0) {
                CVarWindowSize.Set(size);
            }
        }

    #ifdef SP_GRAPHICS_SUPPORT_GL
        Assert(!renderer, "already have a renderer");

        renderer = make_unique<VoxelRenderer>(*glfwContext, timer);

        profilerGui = make_shared<ProfilerGui>(timer);
        if (game->debugGui) {
            game->debugGui->Attach(profilerGui);
        }

        renderer->Prepare();
    #endif

    #ifdef SP_GRAPHICS_SUPPORT_VK
        Assert(!renderer, "already have an active renderer");

        game->debugGui->Attach(make_shared<vulkan::ProfilerGui>(*vkContext->GetPerfTimer()));

        renderer = make_unique<vulkan::Renderer>(*vkContext);
        renderer->SetDebugGui(*game->debugGui.get());
    #endif

        renderStart = chrono_clock::now();
        previousFrameEnd = renderStart;
    }

    bool GraphicsManager::HasActiveContext() {
        return context && !context->ShouldClose();
    }

    bool GraphicsManager::Frame() {
        ZoneScoped;
        if (!context) return true;
    #if defined(SP_GRAPHICS_SUPPORT_GL) || defined(SP_GRAPHICS_SUPPORT_VK)
        if (!HasActiveContext()) return false;
        Assert(renderer, "missing renderer");
    #endif

    #ifdef SP_INPUT_SUPPORT_GLFW
        if (glfwInputHandler) glfwInputHandler->Frame();
    #endif

    #if defined(SP_GRAPHICS_SUPPORT_GL)
        renderer->PrepareGuis(game->debugGui.get(), game->menuGui.get());
    #endif

        if (game->debugGui) game->debugGui->BeforeFrame();
        if (game->menuGui) game->menuGui->BeforeFrame();

        if (!flatviewEntity || CVarFlatviewEntity.Changed()) {
            ecs::Name flatviewName(CVarFlatviewEntity.Get(true), ecs::Name());
            if (flatviewName) flatviewEntity = flatviewName;
        }

        {
            ZoneScopedN("SyncWindowView");
            auto lock = ecs::World.StartTransaction<ecs::Read<ecs::Name>, ecs::Write<ecs::View>>();

            auto flatview = flatviewEntity.Get(lock);
            if (flatview.Has<ecs::View>(lock)) {
                auto &view = flatview.Get<ecs::View>(lock);
                context->PrepareWindowView(view);
            }
            context->AttachView(flatview);
        }

    #ifdef SP_XR_SUPPORT
        auto xrSystem = game->xr.GetXrSystem();
        if (xrSystem) xrSystem->WaitFrame();
    #endif

    #ifdef SP_GRAPHICS_SUPPORT_VK
        #ifdef SP_XR_SUPPORT
        renderer->SetXRSystem(xrSystem);
        #endif

        context->BeginFrame();

        chrono_clock::duration frameInterval = std::chrono::microseconds(0);
        auto maxFPS = CVarMaxFPS.Get();
        if (maxFPS > 0) frameInterval = std::chrono::microseconds(1000000 / maxFPS);

        #ifdef SP_TEST_MODE
        if (stepCount < maxStepCount) {
            renderer->RenderFrame(frameInterval * stepCount.load());
            stepCount++;
            stepCount.notify_all();
        }
        #else
        renderer->RenderFrame(chrono_clock::now() - renderStart);
        #endif

        #ifndef SP_GRAPHICS_SUPPORT_HEADLESS
        context->SwapBuffers();
        #endif
        renderer->EndFrame();
        context->EndFrame();

        auto realFrameEnd = chrono_clock::now();
        auto frameEnd = previousFrameEnd + frameInterval;

        if (frameEnd > realFrameEnd) {
            ZoneScopedN("SleepUntilFrameEnd");
            std::this_thread::sleep_until(frameEnd);
            previousFrameEnd = frameEnd;
        } else {
            previousFrameEnd = realFrameEnd;
        }

        #ifdef SP_XR_SUPPORT
        renderer->SetXRSystem(nullptr);
        #endif
        return true;
    #endif

    #ifdef SP_GRAPHICS_SUPPORT_GL
        timer.StartFrame();
        context->BeginFrame();

        GlfwGraphicsContext *glContext = dynamic_cast<GlfwGraphicsContext *>(context.get());
        Assert(glContext, "GraphicsManager::Frame(): GraphicsContext is not a GlfwGraphicsContext");

        {
            RenderPhase phase("Frame", timer);

            auto lock = ecs::World.StartTransaction<ecs::Read<ecs::Name, ecs::TransformSnapshot, ecs::XRView>,
                ecs::Write<ecs::Renderable, ecs::View, ecs::Light, ecs::LightSensor, ecs::Mirror, ecs::VoxelArea>>();
            renderer->BeginFrame(lock);

            auto flatview = flatviewEntity.Get();
            if (flatview.Has<ecs::View>(lock)) {
                auto &view = flatview.Get<ecs::View>(lock);
                view.UpdateViewMatrix(lock, flatview);
                renderer->RenderPass(view, lock);
            }

        #ifdef SP_XR_SUPPORT
            if (xrSystem) {
                RenderPhase xrPhase("XrViews", timer);

                auto xrViews = lock.EntitiesWith<ecs::XRView>();
                xrRenderTargets.resize(xrViews.size());
                xrRenderPoses.resize(xrViews.size());
                for (size_t i = 0; i < xrViews.size(); i++) {
                    if (!xrViews[i].Has<ecs::View, ecs::XRView>(lock)) continue;
                    auto &view = xrViews[i].Get<ecs::View>(lock);
                    auto &eye = xrViews[i].Get<ecs::XRView>(lock).eye;

                    RenderPhase xrSubPhase("XrView", timer);

                    RenderTargetDesc desc = {PF_SRGB8_A8, view.extents};
                    if (!xrRenderTargets[i] || xrRenderTargets[i]->GetDesc() != desc) {
                        xrRenderTargets[i] = glContext->GetRenderTarget(desc);
                    }

                    view.UpdateViewMatrix(lock, xrViews[i]);
                    if (xrSystem->GetPredictedViewPose(eye, xrRenderPoses[i])) {
                        view.viewMat = glm::inverse(xrRenderPoses[i]) * view.viewMat;
                        view.invViewMat = view.invViewMat * xrRenderPoses[i];
                    }

                    renderer->RenderPass(view, lock, xrRenderTargets[i].get());
                }

                for (size_t i = 0; i < xrViews.size(); i++) {
                    if (!xrViews[i].Has<ecs::View, ecs::XRView>(lock)) continue;
                    RenderPhase xrSubPhase("XrViewSubmit", timer);

                    auto &eye = xrViews[i].Get<ecs::XRView>(lock).eye;
                    xrSystem->SubmitView(eye, xrRenderPoses[i], xrRenderTargets[i]->GetTexture());
                }
            }
        #endif
        }

        context->SwapBuffers();

        renderer->EndFrame();
        timer.EndFrame();
        context->EndFrame();
        return true;
    #endif
    }

    GraphicsContext *GraphicsManager::GetContext() {
        return context.get();
    }
} // namespace sp

#endif
