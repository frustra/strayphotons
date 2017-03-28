#include "graphics/GraphicsManager.hh"
#include "core/Logging.hh"
#include "graphics/Renderer.hh"
#include "graphics/raytrace/RaytracedRenderer.hh"
#include "graphics/basic_renderer/BasicRenderer.hh"
#include "graphics/GuiRenderer.hh"
#include "graphics/GPUTimer.hh"
#include "ecs/components/View.hh"
#include "ecs/components/Transform.hh"
#include "core/Game.hh"
#include "core/CVar.hh"

#include <cxxopts.hpp>
#include <iostream>
#include <system_error>

//#define SP_ENABLE_RAYTRACER

namespace sp
{
	CVar<glm::ivec2> CVarWindowSize("r.Size", { 1600, 900 }, "Window height");
	CVar<float> CVarWindowScale("r.Scale", 1.0f, "Scale framebuffer");
	CVar<float> CVarFieldOfView("r.FieldOfView", 60, "Camera field of view");
	CVar<int> CVarWindowFullscreen("r.Fullscreen", false, "Fullscreen window (0: window, 1: fullscreen)");

#ifdef SP_ENABLE_RAYTRACER
	static CVar<int> CVarRayTrace("r.RayTrace", false, "Run reference raytracer");
#endif

	static void glfwErrorCallback(int error, const char *message)
	{
		Errorf("GLFW returned %d: %s", error, message);
	}

	GraphicsManager::GraphicsManager(Game *game) : game(game)
	{
		if (game->options.count("basic-renderer"))
		{
			Logf("Graphics starting up (basic renderer)");
			useBasic = true;
		}
		else
		{
			Logf("Graphics starting up (full renderer)");
		}

		glfwSetErrorCallback(glfwErrorCallback);

		if (!glfwInit())
		{
			throw "glfw failed";
		}
	}

	GraphicsManager::~GraphicsManager()
	{
		if (context) ReleaseContext();
		glfwTerminate();
	}

	void GraphicsManager::CreateContext()
	{
		if (context) throw "already an active context";

		if (useBasic)
		{
			context = new BasicRenderer(game);
			context->CreateWindow(CVarWindowSize.Get());
			return;
		}

		auto renderer = new Renderer(game);
		context = renderer;
		context->CreateWindow(CVarWindowSize.Get());

#ifdef SP_ENABLE_RAYTRACER
		rayTracer = new raytrace::RaytracedRenderer(game, renderer);
#endif

		profilerGui = new ProfilerGui(context->Timer);
		game->debugGui.Attach(profilerGui);
	}

	void GraphicsManager::ReleaseContext()
	{
		if (!context) throw "no active context";

		if (profilerGui) delete profilerGui;

#ifdef SP_ENABLE_RAYTRACER
		if (rayTracer) delete rayTracer;
#endif

		delete context;
	}

	void GraphicsManager::ReloadContext()
	{
		//context->Reload();
	}

	bool GraphicsManager::HasActiveContext()
	{
		return context && !context->ShouldClose();
	}

	void GraphicsManager::BindContextInputCallbacks(InputManager &inputManager)
	{
		context->BindInputCallbacks(inputManager);
	}

	bool GraphicsManager::Frame()
	{
		if (!context) throw "no active context";
		if (!HasActiveContext()) return false;

		vector<ecs::View> views;
		if (playerViews.size() > 0)
		{
			auto view = playerViews[0].Get<ecs::View>();
			view->extents = CVarWindowSize.Get();
			view->fov = glm::radians(CVarFieldOfView.Get());
			view->scale = CVarWindowScale.Get();

			views.push_back(*ecs::UpdateViewCache(playerViews[0]));
		}
		else
		{
			ecs::View defaultView;
			defaultView.extents = CVarWindowSize.Get();
			views.push_back(defaultView);
		}
		for (size_t i = 1; i < playerViews.size(); i++)
		{
			views.push_back(*ecs::UpdateViewCache(playerViews[i]));
		}

		context->ResizeWindow(views[0], CVarWindowScale.Get(), CVarWindowFullscreen.Get());

		context->Timer->StartFrame();

		{
			RenderPhase phase("Frame", context->Timer);
			context->BeginFrame();

#ifdef SP_ENABLE_RAYTRACER
			if (CVarRayTrace.Get() && rayTracer->Enable(primaryView))
			{
				rayTracer->Render();
			}
			else
			{
				rayTracer->Disable();
			}
#endif
			for (size_t i = 0; i < std::min(playerViews.size(), views.size()); i++)
			{
				context->RenderPass(views[i]);
			}
			context->EndFrame();
		}

		glfwSwapBuffers(context->GetWindow());
		context->Timer->EndFrame();

		double frameEnd = glfwGetTime();
		fpsTimer += frameEnd - lastFrameEnd;
		frameCounter++;

		if (fpsTimer > 1.0)
		{
			context->SetTitle("STRAY PHOTONS (" + std::to_string(frameCounter) + " FPS)");
			frameCounter = 0;
			fpsTimer = 0;
		}

		lastFrameEnd = frameEnd;
		glfwPollEvents();
		return true;
	}

	/**
	* This View will be used when rendering from the player's viewpoint
	*/
	void GraphicsManager::SetPlayerView(vector<ecs::Entity> entities)
	{
		for (auto entity : entities)
		{
			ecs::ValidateView(entity);
		}
		playerViews = entities;
	}

	void GraphicsManager::RenderLoading()
	{
		if (!context) return;

		ecs::View primaryView;
		if (playerViews.size() > 0) primaryView = *ecs::UpdateViewCache(playerViews[0]);

		primaryView.extents = CVarWindowSize.Get();
		primaryView.blend = true;
		primaryView.clearMode = 0;

		context->RenderLoading(primaryView);
	}
}
