#include "GraphicsManager.hh"

#include "core/CVar.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "game/Game.hh"
#include "graphics/core/GraphicsContext.hh"
#include "graphics/core/Renderer.hh"

#ifdef SP_GRAPHICS_SUPPORT_GL
    #include "graphics/opengl/GlfwGraphicsContext.hh"
    #include "graphics/opengl/PerfTimer.hh"
    #include "graphics/opengl/RenderTargetPool.hh"
    #include "graphics/opengl/basic_renderer/BasicRenderer.hh"
    #include "graphics/opengl/gui/ProfilerGui.hh"
    #include "graphics/opengl/voxel_renderer/VoxelRenderer.hh"
    #include "input/glfw/GlfwActionSource.hh"
#endif

#include <algorithm>
#include <cxxopts.hpp>
#include <iostream>
#include <system_error>

namespace sp {
    GraphicsManager::GraphicsManager(Game *game) : game(game) {
#ifdef SP_GRAPHICS_SUPPORT_GL
        if (game->options.count("basic-renderer")) {
            Logf("Graphics starting up (basic renderer)");
            useBasic = true;
        } else {
            Logf("Graphics starting up (full renderer)");
        }
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
            glfwActionSource->BindAction(INPUT_ACTION_PLAYER_MOVE_FORWARD, INPUT_ACTION_KEYBOARD_KEYS + "/w");
            glfwActionSource->BindAction(INPUT_ACTION_PLAYER_MOVE_BACKWARD, INPUT_ACTION_KEYBOARD_KEYS + "/s");
            glfwActionSource->BindAction(INPUT_ACTION_PLAYER_MOVE_LEFT, INPUT_ACTION_KEYBOARD_KEYS + "/a");
            glfwActionSource->BindAction(INPUT_ACTION_PLAYER_MOVE_RIGHT, INPUT_ACTION_KEYBOARD_KEYS + "/d");
            glfwActionSource->BindAction(INPUT_ACTION_PLAYER_MOVE_JUMP, INPUT_ACTION_KEYBOARD_KEYS + "/space");
            glfwActionSource->BindAction(INPUT_ACTION_PLAYER_MOVE_CROUCH, INPUT_ACTION_KEYBOARD_KEYS + "/control_left");
            glfwActionSource->BindAction(INPUT_ACTION_PLAYER_MOVE_SPRINT, INPUT_ACTION_KEYBOARD_KEYS + "/shift_left");
            glfwActionSource->BindAction(INPUT_ACTION_PLAYER_INTERACT, INPUT_ACTION_KEYBOARD_KEYS + "/e");
            glfwActionSource->BindAction(INPUT_ACTION_PLAYER_INTERACT_ROTATE, INPUT_ACTION_KEYBOARD_KEYS + "/r");

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
        if (useBasic) {
            renderer = new BasicRenderer(game->entityManager);
        } else {
            VoxelRenderer *voxelRenderer = new VoxelRenderer(game->entityManager, *glfwContext, timer);
            renderer = voxelRenderer;

            profilerGui = new ProfilerGui(timer);
            if (game->debugGui) { game->debugGui->Attach(profilerGui); }

            {
                auto lock = game->entityManager.tecs.StartTransaction<ecs::AddRemove>();
                viewRemoval = lock.Watch<ecs::Removed<ecs::View>>();
            }

            voxelRenderer->PrepareGuis(game->debugGui.get(), game->menuGui.get());
        }

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

        {
            auto lock = game->entityManager.tecs.StartTransaction<>();
            ecs::Removed<ecs::View> removedView;
            while (viewRemoval.Poll(lock, removedView)) {
                playerViews.erase(std::remove(playerViews.begin(),
                                              playerViews.end(),
                                              ecs::Entity(&game->entityManager, removedView.entity)),
                                  playerViews.end());
            }
        }

        ecs::View pancakeView; // Only support a single pancakeView (2D window)
        vector<std::pair<ecs::View, ecs::XRView>> xrViews; // Support many xrViews

        if (playerViews.size() > 0) {
            for (size_t i = 0; i < playerViews.size(); i++) {
                auto view = playerViews[i].Get<ecs::View>();

                if (view->viewType == ecs::View::VIEW_TYPE_PANCAKE) {
                    // This claims to be a PancakeView, so we can update it
                    // with the screen geometry
                    context->PopulatePancakeView(*view);

                    pancakeView = *ecs::UpdateViewCache(playerViews[i]);
                } else if (view->viewType == ecs::View::VIEW_TYPE_XR && playerViews[i].Has<ecs::XRView>()) {
                    xrViews.push_back(
                        std::make_pair(*ecs::UpdateViewCache(playerViews[i]), *playerViews[i].Get<ecs::XRView>()));
                }
            }
        } else {
            // Somehow we got here without a view...
            throw std::runtime_error("Asked to render without a view");
            return false;
        }

#ifdef SP_GRAPHICS_SUPPORT_GL
        timer.StartFrame();

        {
            RenderPhase phase("Frame", timer);

            renderer->BeginFrame();

            // Always render XR content first, since this allows the compositor to immediately start work rendering to
            // the HMD Only attempt to render if we have an active XR System
            /*if (game->xr.GetXrSystem()) {
                RenderPhase xrPhase("XrViews", timer);

                // TODO: Should not have to do this on every frame...
                glm::mat4 vrOrigin;
                {
                    auto lock = game->entityManager.tecs.StartTransaction<ecs::Read<ecs::Name, ecs::Transform>>();
                    auto vrOriginEntity = ecs::EntityWith<ecs::Name>(lock, "vr-origin");

                    auto &vrOriginTransform = vrOriginEntity.Get<ecs::Transform>(lock);
                    vrOrigin = glm::transpose(vrOriginTransform.GetGlobalTransform(lock));
                }

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
                    xrViews[i].first.SetInvViewMat(viewPose);

                    RenderTarget::Ref viewOutputTexture = game->xr.GetXrSystem()->GetCompositor()->GetRenderTarget(
                        xrViews[i].second.viewId);

                    renderer->RenderPass(xrViews[i].first, viewOutputTexture);

                    game->xr.GetXrSystem()->GetCompositor()->SubmitView(xrViews[i].second.viewId, viewOutputTexture);
                }

                game->xr.GetXrSystem()->GetCompositor()->EndFrame();
            }*/

            // Render the 2D pancake view
            context->PrepareForView(pancakeView);
            renderer->RenderPass(pancakeView);

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

    void GraphicsManager::AddPlayerView(ecs::Entity entity) {
        ecs::ValidateView(entity);

        playerViews.push_back(entity);
    }

    void GraphicsManager::AddPlayerView(Tecs::Entity entity) {
        playerViews.push_back(ecs::Entity(&game->entityManager, entity));
    }

    void GraphicsManager::RenderLoading() {
        if (!renderer) return;

        if (playerViews.size() > 0) {
            for (size_t i = 0; i < playerViews.size(); i++) {
                auto view = playerViews[i].Get<ecs::View>();

                if (view->viewType == ecs::View::VIEW_TYPE_PANCAKE) {
                    // This claims to be a PancakeView, so we can update it
                    // with the screen geometry
                    context->PopulatePancakeView(*view);

                    ecs::View pancakeView = *ecs::UpdateViewCache(playerViews[i]);
                    pancakeView.blend = true;
                    pancakeView.clearMode.reset();

                    renderer->RenderLoading(pancakeView);
                    context->SwapBuffers();
                }
            }
        }

        // TODO: clear the XR scene to drop back to the compositor while we load
    }
} // namespace sp
