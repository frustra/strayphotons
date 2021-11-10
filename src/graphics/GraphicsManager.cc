#ifdef SP_GRAPHICS_SUPPORT
    #include "GraphicsManager.hh"

    #include "console/CVar.hh"
    #include "core/Logging.hh"
    #include "ecs/EcsImpl.hh"
    #include "game/Game.hh"
    #include "graphics/core/GraphicsContext.hh"

    #ifdef SP_GRAPHICS_SUPPORT_GL
        #include "graphics/opengl/GlfwGraphicsContext.hh"
        #include "graphics/opengl/PerfTimer.hh"
        #include "graphics/opengl/RenderTargetPool.hh"
        #include "graphics/opengl/gui/ProfilerGui.hh"
        #include "graphics/opengl/voxel_renderer/VoxelRenderer.hh"
        #include "input/glfw/GlfwInputHandler.hh"
    #endif
    #ifdef SP_GRAPHICS_SUPPORT_VK
        #include "graphics/vulkan/Renderer.hh"
        #include "graphics/vulkan/core/DeviceContext.hh"
    #endif

    #include <algorithm>
    #include <cxxopts.hpp>
    #include <glm/gtc/matrix_access.hpp>
    #include <iostream>
    #include <optional>
    #include <system_error>
    #include <thread>

namespace sp {
    GraphicsManager::GraphicsManager(Game *game) : game(game) {
    #ifdef SP_GRAPHICS_SUPPORT_GL
        Logf("Graphics starting up (OpenGL)");
    #endif
    #ifdef SP_GRAPHICS_SUPPORT_VK
        Logf("Graphics starting up (Vulkan)");
    #endif
    }

    GraphicsManager::~GraphicsManager() {
        if (context) context->WaitIdle();
    }

    void GraphicsManager::Init() {
        Assert(!context, "already have a graphics context");

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

        if (window != nullptr) game->glfwInputHandler = make_unique<GlfwInputHandler>(*window);

        if (game->options.count("size")) {
            std::istringstream ss(game->options["size"].as<string>());
            glm::ivec2 size;
            ss >> size.x >> size.y;

            if (size.x > 0 && size.y > 0) { CVarWindowSize.Set(size); }
        }

    #ifdef SP_GRAPHICS_SUPPORT_GL
        Assert(!renderer, "already have a renderer");

        renderer = make_unique<VoxelRenderer>(*glfwContext, timer);

        profilerGui = make_shared<ProfilerGui>(timer);
        if (game->debugGui) { game->debugGui->Attach(profilerGui); }

        renderer->Prepare();
    #endif

    #ifdef SP_GRAPHICS_SUPPORT_VK
        Assert(!renderer, "already have an active renderer");

        renderer = make_unique<vulkan::Renderer>(*vkContext);
        renderer->SetDebugGui(*game->debugGui.get());
    #endif
    }

    bool GraphicsManager::HasActiveContext() {
        return context && !context->ShouldClose();
    }

    bool GraphicsManager::Frame() {
    #if defined(SP_GRAPHICS_SUPPORT_GL) || defined(SP_GRAPHICS_SUPPORT_VK)
        Assert(context, "missing graphics context");
        if (!HasActiveContext()) return false;
        Assert(renderer, "missing renderer");
    #endif

    #if defined(SP_GRAPHICS_SUPPORT_GL)
        renderer->PrepareGuis(game->debugGui.get(), game->menuGui.get());
    #endif

        if (game->debugGui) game->debugGui->BeforeFrame();
        if (game->menuGui) game->menuGui->BeforeFrame();

        Tecs::Entity windowEntity = context->GetActiveView();
        if (windowEntity) {
            auto lock = ecs::World.StartTransaction<ecs::Write<ecs::View>>();

            if (windowEntity.Has<ecs::View>(lock)) {
                auto &view = windowEntity.Get<ecs::View>(lock);
                view.visibilityMask.set(ecs::Renderable::VISIBLE_DIRECT_CAMERA);
                context->PrepareWindowView(view);
            }
        }

    #ifdef SP_XR_SUPPORT
        auto xrSystem = game->xr.GetXrSystem();

        #ifdef SP_GRAPHICS_SUPPORT_VK
        renderer->SetXRSystem(xrSystem.get());
        #endif

        if (xrSystem) xrSystem->WaitFrame();
    #endif

    #ifdef SP_GRAPHICS_SUPPORT_VK
        // timer.StartFrame();
        context->BeginFrame();
        renderer->RenderFrame();
        #ifndef SP_GRAPHICS_SUPPORT_HEADLESS
        context->SwapBuffers();
        #else
        // TODO: Configurable headless frame rate
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        #endif
        // timer.EndFrame();
        context->EndFrame();
        return true;
    #endif

    #ifdef SP_GRAPHICS_SUPPORT_GL
        timer.StartFrame();
        context->BeginFrame();

        GlfwGraphicsContext *glContext = dynamic_cast<GlfwGraphicsContext *>(context.get());
        Assert(glContext != nullptr, "GraphicsManager::Frame(): GraphicsContext is not a GlfwGraphicsContext");

        {
            RenderPhase phase("Frame", timer);

            auto lock = ecs::World.StartTransaction<ecs::Read<ecs::Name, ecs::Transform, ecs::XRView>,
                ecs::Write<ecs::Renderable, ecs::View, ecs::Light, ecs::LightSensor, ecs::Mirror, ecs::VoxelArea>>();
            renderer->BeginFrame(lock);

            if (windowEntity && windowEntity.Has<ecs::View>(lock)) {
                auto &view = windowEntity.Get<ecs::View>(lock);
                view.UpdateViewMatrix(lock, windowEntity);
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

            renderer->EndFrame();
        }

        context->SwapBuffers();

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
