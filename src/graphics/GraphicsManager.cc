#ifdef SP_GRAPHICS_SUPPORT

    #include "GraphicsManager.hh"

    #include "core/CVar.hh"
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
        #include "input/glfw/GlfwActionSource.hh"
    #endif

    #ifdef SP_PHYSICS_SUPPORT_PHYSX
        #include "physx/HumanControlSystem.hh"
    #endif

    #include <algorithm>
    #include <cxxopts.hpp>
    #include <iostream>
    #include <system_error>

namespace sp {
    GraphicsManager::GraphicsManager(Game *game) : game(game) {
    #ifdef SP_GRAPHICS_SUPPORT_GL
        Logf("Graphics starting up (OpenGL)");
    #endif
    }

    GraphicsManager::~GraphicsManager() {
    #ifdef SP_GRAPHICS_SUPPORT_GL
        if (renderer) { delete renderer; }

        if (profilerGui) { delete profilerGui; }

        if (glfwActionSource) { delete glfwActionSource; }

        if (context) { delete context; }
    #endif
    }

    void GraphicsManager::Init() {
    #ifdef SP_GRAPHICS_SUPPORT_GL
        if (context) { throw "already an active context"; }
        if (renderer) { throw "already an active renderer"; }

        GlfwGraphicsContext *glfwContext = new GlfwGraphicsContext();
        context = glfwContext;
        context->Init();

        GLFWwindow *window = glfwContext->GetWindow();
        if (window != nullptr) {
            glfwActionSource = new GlfwActionSource(game->input, *window);

                // TODO: Expose some sort of configuration for these.
        #ifdef SP_PHYSICS_SUPPORT_PHYSX
            glfwActionSource->BindAction(INPUT_ACTION_PLAYER_MOVE_FORWARD, INPUT_ACTION_KEYBOARD_KEYS + "/w");
            glfwActionSource->BindAction(INPUT_ACTION_PLAYER_MOVE_BACKWARD, INPUT_ACTION_KEYBOARD_KEYS + "/s");
            glfwActionSource->BindAction(INPUT_ACTION_PLAYER_MOVE_LEFT, INPUT_ACTION_KEYBOARD_KEYS + "/a");
            glfwActionSource->BindAction(INPUT_ACTION_PLAYER_MOVE_RIGHT, INPUT_ACTION_KEYBOARD_KEYS + "/d");
            glfwActionSource->BindAction(INPUT_ACTION_PLAYER_MOVE_JUMP, INPUT_ACTION_KEYBOARD_KEYS + "/space");
            glfwActionSource->BindAction(INPUT_ACTION_PLAYER_MOVE_CROUCH, INPUT_ACTION_KEYBOARD_KEYS + "/control_left");
            glfwActionSource->BindAction(INPUT_ACTION_PLAYER_MOVE_SPRINT, INPUT_ACTION_KEYBOARD_KEYS + "/shift_left");
            glfwActionSource->BindAction(INPUT_ACTION_PLAYER_INTERACT, INPUT_ACTION_KEYBOARD_KEYS + "/e");
            glfwActionSource->BindAction(INPUT_ACTION_PLAYER_INTERACT_ROTATE, INPUT_ACTION_KEYBOARD_KEYS + "/r");
        #endif

            glfwActionSource->BindAction(INPUT_ACTION_OPEN_MENU, INPUT_ACTION_KEYBOARD_KEYS + "/escape");
            glfwActionSource->BindAction(INPUT_ACTION_TOGGLE_CONSOLE, INPUT_ACTION_KEYBOARD_KEYS + "/backtick");
            glfwActionSource->BindAction(INPUT_ACTION_MENU_ENTER, INPUT_ACTION_KEYBOARD_KEYS + "/enter");
            glfwActionSource->BindAction(INPUT_ACTION_MENU_BACK, INPUT_ACTION_KEYBOARD_KEYS + "/escape");

            glfwActionSource->BindAction(INPUT_ACTION_SPAWN_DEBUG, INPUT_ACTION_KEYBOARD_KEYS + "/q");
            glfwActionSource->BindAction(INPUT_ACTION_TOGGLE_FLASHLIGH, INPUT_ACTION_KEYBOARD_KEYS + "/f");
            glfwActionSource->BindAction(INPUT_ACTION_DROP_FLASHLIGH, INPUT_ACTION_KEYBOARD_KEYS + "/p");

            glfwActionSource->BindAction(INPUT_ACTION_SET_VR_ORIGIN, INPUT_ACTION_KEYBOARD_KEYS + "/f1");
            glfwActionSource->BindAction(INPUT_ACTION_RELOAD_SCENE, INPUT_ACTION_KEYBOARD_KEYS + "/f5");
            glfwActionSource->BindAction(INPUT_ACTION_RESET_SCENE, INPUT_ACTION_KEYBOARD_KEYS + "/f6");
            glfwActionSource->BindAction(INPUT_ACTION_RELOAD_SHADERS, INPUT_ACTION_KEYBOARD_KEYS + "/f7");
        }
    #endif

        if (game->options.count("size")) {
            std::istringstream ss(game->options["size"].as<string>());
            glm::ivec2 size;
            ss >> size.x >> size.y;

            if (size.x > 0 && size.y > 0) { CVarWindowSize.Set(size); }
        }

    #ifdef SP_GRAPHICS_SUPPORT_GL
        {
            auto lock = ecs::World.StartTransaction<ecs::AddRemove>();
            renderer = new VoxelRenderer(lock, *glfwContext, timer);
        }

        profilerGui = new ProfilerGui(timer);
        if (game->debugGui) { game->debugGui->Attach(profilerGui); }

        renderer->PrepareGuis(game->debugGui.get(), game->menuGui.get());

        renderer->Prepare();
    #endif
    }

    bool GraphicsManager::HasActiveContext() {
        return context && !context->ShouldClose();
    }

    bool GraphicsManager::Frame() {
    #ifdef SP_GRAPHICS_SUPPORT_GL
        if (!context) throw "no active context";
        if (!renderer) throw "no active renderer";
        if (!HasActiveContext()) return false;
    #endif

        std::vector<ecs::View> cameraViews;
        std::vector<ecs::View> shadowViews;
        std::vector<std::pair<ecs::View, ecs::XRView>> xrViews;
        {
            auto lock = ecs::World.StartTransaction<ecs::Read<ecs::Transform, ecs::Light, ecs::XRView>,
                                                    ecs::Write<ecs::View>>();

            auto &windowEntity = context->GetActiveView();
            if (windowEntity) {
                if (windowEntity.Has<ecs::View>(lock)) {
                    auto &windowView = windowEntity.Get<ecs::View>(lock);
                    windowView.visibilityMask.set(ecs::Renderable::VISIBILE_DIRECT_CAMERA);
                    context->PrepareWindowView(windowView);
                }
            }

            for (auto &e : lock.EntitiesWith<ecs::Light>()) {
                if (e.Has<ecs::Light, ecs::View>(lock)) {
                    auto &view = e.Get<ecs::View>(lock);
                    view.visibilityMask.set(ecs::Renderable::VISIBILE_LIGHTING_SHADOW);
                }
            }

            for (auto &e : lock.EntitiesWith<ecs::View>()) {
                auto &view = e.Get<ecs::View>(lock);
                view.UpdateMatrixCache(lock, e);
                if (view.visibilityMask[ecs::Renderable::VISIBILE_DIRECT_CAMERA]) {
                    cameraViews.emplace_back(view);
                } else if (view.visibilityMask[ecs::Renderable::VISIBILE_DIRECT_EYE] && e.Has<ecs::XRView>(lock)) {
                    xrViews.emplace_back(view, e.Get<ecs::XRView>(lock));
                } else if (view.visibilityMask[ecs::Renderable::VISIBILE_LIGHTING_SHADOW]) {
                    shadowViews.emplace_back(view);
                }
            }
        }

    #ifdef SP_GRAPHICS_SUPPORT_GL
        timer.StartFrame();

        {
            RenderPhase phase("Frame", timer);

            auto lock = ecs::World.StartTransaction<
                ecs::Read<ecs::Name, ecs::Transform>,
                ecs::Write<ecs::Renderable, ecs::View, ecs::Light, ecs::LightSensor, ecs::Mirror, ecs::VoxelArea>>();
            renderer->BeginFrame(lock);

        #ifdef SP_XR_SUPPORT
            // Always render XR content first, since this allows the compositor to immediately start work rendering to
            // the HMD Only attempt to render if we have an active XR System
            if (game->xr.GetXrSystem()) {
                RenderPhase xrPhase("XrViews", timer);

                // TODO: Should not have to do this on every frame...
                auto vrOriginEntity = ecs::EntityWith<ecs::Name>(lock, "vr-origin");

                auto &vrOriginTransform = vrOriginEntity.Get<ecs::Transform>(lock);
                glm::mat4 vrOrigin = glm::transpose(vrOriginTransform.GetGlobalTransform(lock));

                {
                    RenderPhase xrPhase("XrWaitFrame", timer);
                    // Wait for the XR system to be ready to accept a new frame
                    game->xr.GetXrSystem()->GetCompositor()->WaitFrame();
                }

                // Tell the XR system we are about to begin rendering
                game->xr.GetXrSystem()->GetCompositor()->BeginFrame();

                // Render all the XR views at the same time
                for (size_t i = 0; i < xrViews.size(); i++) {
                    RenderPhase xrPhase("XrView", timer);

                    static glm::mat4 viewPose;
                    game->xr.GetXrSystem()->GetTracking()->GetPredictedViewPose(xrViews[i].second.viewId, viewPose);

                    // Calculate the view pose relative to the current vrOrigin
                    viewPose = glm::transpose(viewPose * vrOrigin);

                    // Move the view to the appropriate place
                    // TODO(xthexder): Fix XR view positioning
                    // xrViews[i].first.SetInvViewMat(viewPose);

                    RenderTarget *viewOutputTexture = game->xr.GetXrSystem()->GetCompositor()->GetRenderTarget(
                        xrViews[i].second.viewId);

                    renderer->RenderPass(xrViews[i].first, lock, viewOutputTexture);

                    game->xr.GetXrSystem()->GetCompositor()->SubmitView(xrViews[i].second.viewId,
                                                                        viewOutputTexture->GetTexture());
                }

                game->xr.GetXrSystem()->GetCompositor()->EndFrame();
            }
        #endif

            for (auto &view : cameraViews) {
                renderer->RenderPass(view, lock);
            }

            renderer->EndFrame();
        }

        context->SwapBuffers();
        timer.EndFrame();
        context->EndFrame();
    #endif

        return true;
    }

    GraphicsContext *GraphicsManager::GetContext() {
        return context;
    }

    void GraphicsManager::DisableCursor() {
    #ifdef SP_GRAPHICS_SUPPORT_GL
        if (glfwActionSource) { glfwActionSource->DisableCursor(); }
    #endif
    }

    void GraphicsManager::EnableCursor() {
    #ifdef SP_GRAPHICS_SUPPORT_GL
        if (glfwActionSource) { glfwActionSource->EnableCursor(); }
    #endif
    }

    void GraphicsManager::RenderLoading() {
        if (!renderer) return;

        auto lock = ecs::World.StartTransaction<ecs::Read<ecs::View>>();

        auto &windowEntity = context->GetActiveView();
        if (windowEntity && windowEntity.Has<ecs::View>(lock)) {
            auto windowView = windowEntity.Get<ecs::View>(lock);
            windowView.blend = true;
            windowView.clearMode.reset();

            renderer->RenderLoading(windowView);
            context->SwapBuffers();
        }

        // TODO: clear the XR scene to drop back to the compositor while we load
    }
} // namespace sp

#endif
