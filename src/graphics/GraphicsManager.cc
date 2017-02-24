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
#include "game/gui/ProfilerGui.hh"

#include <cxxopts.hpp>
#include <iostream>
#include <system_error>

//#define SP_ENABLE_RAYTRACER

namespace sp
{
	static CVar<glm::ivec2> CVarWindowSize("r.Size", { 1600, 900 }, "Window height");
	static CVar<float> CVarFieldOfView("r.FieldOfView", 70, "Camera field of view");
	static CVar<int> CVarWindowFullscreen("r.Fullscreen", false, "Fullscreen window (0: window, 1: fullscreen)");

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

		guiRenderer = new GuiRenderer(*renderer, game->gui);

#ifdef SP_ENABLE_RAYTRACER
		rayTracer = new raytrace::RaytracedRenderer(game, renderer);
#endif

		game->gui.Attach(new ProfilerGui(context->Timer));
	}

	void GraphicsManager::ReleaseContext()
	{
		if (!context) throw "no active context";

		if (guiRenderer) delete guiRenderer;

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

		ecs::View primaryView;
		if (playerView.Valid())
		{
			auto newSize = CVarWindowSize.Get();
			auto newFov = CVarFieldOfView.Get();

			if (newSize != primaryView.extents || newFov != primaryView.fov)
			{
				auto view = playerView.Get<ecs::View>();
				view->extents = newSize;
				view->fov = newFov;
			}

			primaryView = *ecs::UpdateViewCache(playerView);
			primaryView.clearMode |= GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT;
		}
		else
		{
			// Default view
			primaryView.extents = CVarWindowSize.Get();
			// primaryView.fov = CVarFieldOfView.Get();
		}

		context->ResizeWindow(primaryView, CVarWindowFullscreen.Get());

		context->Timer->StartFrame();
		context->BeginFrame();

		{
			RenderPhase phase("Frame", context->Timer);

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

			context->RenderPass(primaryView);

			if (guiRenderer)
			{
				guiRenderer->Render(primaryView);
			}
		}

		context->EndFrame();
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
	void GraphicsManager::SetPlayerView(ecs::Entity entity)
	{
		ecs::ValidateView(entity);
		playerView = entity;
	}
}
