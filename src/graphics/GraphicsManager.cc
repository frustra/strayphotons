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
	static CVar<glm::ivec2> CVarWindowSize("r.Size", { 0, 0 }, "Window height");
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

		auto primaryView = updateViewCaches(playerView);

		if (useBasic)
		{
			context = new BasicRenderer(game);
			context->CreateWindow(primaryView.extents);
			return;
		}

		auto renderer = new Renderer(game);
		context = renderer;
		context->CreateWindow(primaryView.extents);

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

		auto primaryView = updateViewCaches(playerView);
		primaryView.clearMode |= GL_COLOR_BUFFER_BIT;

		{
			auto newSize = CVarWindowSize.Get();

			if (newSize.x == 0 || newSize.y == 0)
			{
				CVarWindowSize.Set(primaryView.extents);
			}
			else if (newSize != primaryView.extents)
			{
				playerView.Get<ecs::View>()->extents = newSize;
				primaryView = updateViewCaches(playerView);
			}
		}

		context->Timer->StartFrame();
		context->BeginFrame(primaryView, CVarWindowFullscreen.Get());

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
		validateView(entity);
		playerView = entity;
	}

	ecs::View &GraphicsManager::updateViewCaches(ecs::Entity entity)
	{
		validateView(entity);

		auto view = entity.Get<ecs::View>();

		view->aspect = (float)view->extents.x / (float)view->extents.y;
		view->projMat = glm::perspective(view->fov, view->aspect, view->clip[0], view->clip[1]);
		view->invProjMat = glm::inverse(view->projMat);

		auto transform = entity.Get<ecs::Transform>();
		view->invViewMat = transform->GetModelTransform(*entity.GetManager());
		view->viewMat = glm::inverse(view->invViewMat);

		return *view;
	}

	void GraphicsManager::validateView(ecs::Entity viewEntity)
	{
		if (!viewEntity.Valid())
		{
			throw std::runtime_error("view entity is not valid because the entity has been deleted");
		}
		if (!viewEntity.Has<ecs::View>())
		{
			throw std::runtime_error("view entity is not valid because it has no View component");
		}
		if (!viewEntity.Has<ecs::Transform>())
		{
			throw std::runtime_error("view entity is not valid because it has no Transform component");
		}
	}
}
