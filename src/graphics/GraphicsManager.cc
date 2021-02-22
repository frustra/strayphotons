#include "graphics/GraphicsManager.hh"

#include "core/CVar.hh"
#include "core/Game.hh"
#include "core/Logging.hh"
#include "core/PerfTimer.hh"
#include "graphics/RenderTargetPool.hh"
#include "graphics/glfw_graphics_context/GlfwGraphicsContext.hh"
#include "graphics/voxel_renderer/VoxelRenderer.hh"
#include "graphics/basic_renderer/BasicRenderer.hh"

#include "graphics/Renderer.hh"

#include <algorithm>
#include <cxxopts.hpp>
#include <ecs/EcsImpl.hh>
#include <iostream>
#include <system_error>

namespace sp {
    GraphicsManager::GraphicsManager(Game *game) : game(game) {
        if (game->options.count("basic-renderer")) {
            Logf("Graphics starting up (basic renderer)");
            useBasic = true;
        } else {
            Logf("Graphics starting up (full renderer)");
        }
    }

    GraphicsManager::~GraphicsManager() {

        if (renderer) {
            delete renderer;
        }

        if (profilerGui) {
            delete profilerGui;
        }

        if (context) {
            delete context;
        }
    }

    void GraphicsManager::Init() {
        if (context) {
            throw "already an active context";
        }

        if (renderer) {
            throw "already an active renderer";
        }

        context = new GlfwGraphicsContext(game);

        if (useBasic) {
            renderer = new BasicRenderer(game);
        } else {
            renderer = new VoxelRenderer(game, *context);

            profilerGui = new ProfilerGui(&renderer->Timer);
            if (game->debugGui) { 
                game->debugGui->Attach(profilerGui); 
            }

            {
                auto lock = game->entityManager.tecs.StartTransaction<ecs::AddRemove>();
                viewRemoval = lock.Watch<ecs::Removed<ecs::View>>();
            }
        }

        context->Init();
        renderer->Prepare();
    }

    bool GraphicsManager::HasActiveContext() {
        return context && !context->ShouldClose();
    }

    bool GraphicsManager::Frame() {
        if (!context) throw "no active context";
        if (!renderer) throw "no active renderer";
        if (!HasActiveContext()) return false;

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

        renderer->Timer.StartFrame();

        {
            RenderPhase phase("Frame", renderer->Timer);

            renderer->BeginFrame();

            // Always render XR content first, since this allows the compositor to immediately start work rendering to
            // the HMD Only attempt to render if we have an active XR System
            if (game->xr.GetXrSystem()) {
                RenderPhase xrPhase("XrViews", renderer->Timer);

                // TODO: Should not have to do this on every frame...
                glm::mat4 vrOrigin;
                {
                    auto lock = game->entityManager.tecs.StartTransaction<ecs::Read<ecs::Name, ecs::Transform>>();
                    auto vrOriginEntity = ecs::EntityWith<ecs::Name>(lock, "vr-origin");

                    auto &vrOriginTransform = vrOriginEntity.Get<ecs::Transform>(lock);
                    vrOrigin = glm::transpose(vrOriginTransform.GetGlobalTransform(lock));
                }

                {
                    RenderPhase xrPhase("XrWaitFrame", renderer->Timer);
                    // Wait for the XR system to be ready to accept a new frame
                    game->xr.GetXrSystem()->GetCompositor()->WaitFrame();
                }

                // Tell the XR system we are about to begin rendering
                game->xr.GetXrSystem()->GetCompositor()->BeginFrame();

                // Render all the XR views at the same time
                for (size_t i = 0; i < xrViews.size(); i++) {
                    RenderPhase xrPhase("XrView", renderer->Timer);

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
            }

            // Render the 2D pancake view
            context->PrepareForView(pancakeView);
            renderer->RenderPass(pancakeView);

            renderer->EndFrame();
        }

        context->SwapBuffers();
        renderer->Timer.EndFrame();
        context->EndFrame();
        
        return true;
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
                    pancakeView.clearMode = 0;

                    renderer->RenderLoading(pancakeView);
                    context->SwapBuffers();
                }
            }
        }

        // TODO: clear the XR scene to drop back to the compositor while we load
    }
} // namespace sp
