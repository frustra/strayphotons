#include "graphics/GraphicsManager.hh"

#include "core/CVar.hh"
#include "core/Game.hh"
#include "core/Logging.hh"
#include "core/PerfTimer.hh"
#include "ecs/components/Transform.hh"
#include "ecs/components/View.hh"
#include "ecs/components/XRView.hh"
#include "graphics/GuiRenderer.hh"
#include "graphics/RenderTargetPool.hh"
#include "graphics/Renderer.hh"
#include "graphics/basic_renderer/BasicRenderer.hh"

#include <cxxopts.hpp>
#include <iostream>
#include <system_error>

// clang-format off
// GLFW must be included after glew.h (Graphics.hh)
#include <GLFW/glfw3.h>
// clang-format on

namespace sp {
	CVar<glm::ivec2> CVarWindowSize("r.Size", {1280, 720}, "Window height");
	CVar<float> CVarWindowScale("r.Scale", 1.0f, "Scale framebuffer");
	CVar<float> CVarFieldOfView("r.FieldOfView", 60, "Camera field of view");
	CVar<int> CVarWindowFullscreen("r.Fullscreen", false, "Fullscreen window (0: window, 1: fullscreen)");

	static void glfwErrorCallback(int error, const char *message) {
		Errorf("GLFW returned %d: %s", error, message);
	}

	GraphicsManager::GraphicsManager(Game *game) : game(game) {
		if (game->options.count("basic-renderer")) {
			Logf("Graphics starting up (basic renderer)");
			useBasic = true;
		} else {
			Logf("Graphics starting up (full renderer)");
		}

		if (game->options.count("size")) {
			std::istringstream ss(game->options["size"].as<string>());
			glm::ivec2 size;
			ss >> size.x >> size.y;

			if (size.x > 0 && size.y > 0) {
				CVarWindowSize.Set(size);
			}
		}

		glfwSetErrorCallback(glfwErrorCallback);

		if (!glfwInit()) {
			throw "glfw failed";
		}
	}

	GraphicsManager::~GraphicsManager() {
		if (context)
			ReleaseContext();
		glfwTerminate();
	}

	void GraphicsManager::CreateContext() {
		if (context)
			throw "already an active context";

		if (useBasic) {
			context = new BasicRenderer(game);
			context->CreateWindow(CVarWindowSize.Get());
			return;
		}

		auto renderer = new Renderer(game);
		context = renderer;
		context->CreateWindow(CVarWindowSize.Get());

		profilerGui = new ProfilerGui(&context->Timer);
		if (game->debugGui) {
			game->debugGui->Attach(profilerGui);
		}
	}

	void GraphicsManager::ReleaseContext() {
		if (!context)
			throw "no active context";

		if (profilerGui)
			delete profilerGui;

		delete context;
	}

	void GraphicsManager::ReloadContext() {
		// context->Reload();
	}

	bool GraphicsManager::HasActiveContext() {
		return context && !context->ShouldClose();
	}

	void GraphicsManager::PreFrame() {
		if (context)
			context->PreFrame();
	}

	bool GraphicsManager::Frame() {
		if (!context)
			throw "no active context";
		if (!HasActiveContext())
			return false;

		ecs::View pancakeView;                             // Only support a single pancakeView (2D window)
		vector<std::pair<ecs::View, ecs::XRView>> xrViews; // Support many xrViews

		if (playerViews.size() > 0) {
			for (size_t i = 0; i < playerViews.size(); i++) {
				auto view = playerViews[i].Get<ecs::View>();

				if (view->viewType == ecs::View::VIEW_TYPE_PANCAKE) {
					// This claims to be a PancakeView, so we can update it
					// with the screen geometry
					view->SetProjMat(glm::radians(CVarFieldOfView.Get()), view->GetClip(), CVarWindowSize.Get());
					view->scale = CVarWindowScale.Get();

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

		context->Timer.StartFrame();

		{
			RenderPhase phase("Frame", context->Timer);

			context->BeginFrame();

			// Always render XR content first, since this allows the compositor to immediately start work rendering to
			// the HMD Only attempt to render if we have an active XR System
			if (game->logic.GetXrSystem()) {
				RenderPhase xrPhase("XrViews", context->Timer);

				// TODO: Should not have to do this on every frame...
				ecs::Entity vrOrigin = game->entityManager.EntityWith<ecs::Name>("vr-origin");

				auto vrOriginTransform = vrOrigin.Get<ecs::Transform>();
				auto vrOriginMat4 = glm::transpose(vrOriginTransform->GetGlobalTransform(game->entityManager));

				{
					RenderPhase xrPhase("XrWaitFrame", context->Timer);
					// Wait for the XR system to be ready to accept a new frame
					game->logic.GetXrSystem()->GetCompositor()->WaitFrame();
				}

				// Tell the XR system we are about to begin rendering
				game->logic.GetXrSystem()->GetCompositor()->BeginFrame();

				// Render all the XR views at the same time
				for (size_t i = 0; i < xrViews.size(); i++) {
					RenderPhase xrPhase("XrView", context->Timer);

					static glm::mat4 viewPose;
					game->logic.GetXrSystem()->GetTracking()->GetPredictedViewPose(xrViews[i].second.viewId, viewPose);

					// Calculate the view pose relative to the current vrOrigin
					viewPose = glm::transpose(viewPose * vrOriginMat4);

					// Move the view to the appropriate place
					xrViews[i].first.SetInvViewMat(viewPose);

					RenderTarget::Ref viewOutputTexture =
						game->logic.GetXrSystem()->GetCompositor()->GetRenderTarget(xrViews[i].second.viewId);

					context->RenderPass(xrViews[i].first, viewOutputTexture);

					game->logic.GetXrSystem()->GetCompositor()->SubmitView(xrViews[i].second.viewId, viewOutputTexture);
				}

				game->logic.GetXrSystem()->GetCompositor()->EndFrame();
			}

			// Render the 2D pancake view
			context->ResizeWindow(pancakeView, CVarWindowScale.Get(), CVarWindowFullscreen.Get());
			context->RenderPass(pancakeView);

			context->EndFrame();
		}

		glfwSwapBuffers(context->GetWindow());
		context->Timer.EndFrame();

		double frameEnd = glfwGetTime();
		fpsTimer += frameEnd - lastFrameEnd;
		frameCounter++;

		if (fpsTimer > 1.0) {
			context->SetTitle("STRAY PHOTONS (" + std::to_string(frameCounter) + " FPS)");
			frameCounter = 0;
			fpsTimer = 0;
		}

		lastFrameEnd = frameEnd;
		return true;
	}

	/**
	 * This View will be used when rendering from the player's viewpoint
	 */
	void GraphicsManager::SetPlayerView(vector<ecs::Entity> entities) {
		for (auto entity : entities) { ecs::ValidateView(entity); }
		playerViews = entities;
	}

	void GraphicsManager::RenderLoading() {
		if (!context)
			return;

		if (playerViews.size() > 0) {
			for (size_t i = 0; i < playerViews.size(); i++) {
				auto view = playerViews[i].Get<ecs::View>();

				if (view->viewType == ecs::View::VIEW_TYPE_PANCAKE) {
					// This claims to be a PancakeView, so we can update it
					// with the screen geometry
					view->SetProjMat(glm::radians(CVarFieldOfView.Get()), view->GetClip(), CVarWindowSize.Get());
					view->scale = CVarWindowScale.Get();

					ecs::View pancakeView = *ecs::UpdateViewCache(playerViews[i]);
					pancakeView.blend = true;
					pancakeView.clearMode = 0;

					context->RenderLoading(pancakeView);
				}
			}
		}

		// TODO: clear the XR scene to drop back to the compositor while we load
	}
} // namespace sp
