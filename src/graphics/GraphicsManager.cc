#include "graphics/GraphicsManager.hh"
#include "core/Logging.hh"
#include "graphics/Renderer.hh"
#include "graphics/GuiRenderer.hh"
#include "graphics/GPUTimer.hh"
#include "ecs/components/View.hh"
#include "ecs/components/Transform.hh"
#include "core/Game.hh"
#include "core/CVar.hh"
#include "game/gui/ProfilerGui.hh"

#include <iostream>
#include <system_error>

namespace sp
{
	static CVar<glm::ivec2> CVarWindowSize("r.Size", { 0, 0 }, "Window height");
	static CVar<int> CVarWindowFullscreen("r.Fullscreen", false, "Fullscreen window (0: window, 1: fullscreen)");

	static void glfwErrorCallback(int error, const char *message)
	{
		Errorf("GLFW returned %d: %s", error, message);
	}

	GraphicsManager::GraphicsManager(Game *game) : game(game)
	{
		Logf("Graphics starting up");

		glfwSetErrorCallback(glfwErrorCallback);

		if (!glfwInit())
		{
			throw "glfw failed";
		}
	}

	GraphicsManager::~GraphicsManager()
	{
		if (renderer) ReleaseContext();
		glfwTerminate();
	}

	void GraphicsManager::CreateContext()
	{
		if (renderer) throw "already an active context";

		auto primaryView = updateViewCaches(playerView);

		renderer = new Renderer(game);
		renderer->CreateWindow(primaryView.extents);

		guiRenderer = new GuiRenderer(*renderer, game->gui);

		game->gui.Attach(new ProfilerGui(renderer->timer));
	}

	void GraphicsManager::ReleaseContext()
	{
		if (!renderer) throw "no active context";

		delete renderer;
	}

	void GraphicsManager::ReloadContext()
	{
		//context->Reload();
	}

	bool GraphicsManager::HasActiveContext()
	{
		return renderer && !renderer->ShouldClose();
	}

	void GraphicsManager::BindContextInputCallbacks(InputManager &inputManager)
	{
		renderer->BindInputCallbacks(inputManager);
	}

	bool GraphicsManager::Frame()
	{
		if (!renderer) throw "missing renderer";
		if (!HasActiveContext()) return false;

		auto primaryView = updateViewCaches(playerView);

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

		renderer->BeginFrame(primaryView, CVarWindowFullscreen.Get());
		renderer->timer->StartFrame();

		{
			RenderPhase phase("Frame", renderer->timer);

			auto shadowMap = renderer->RenderShadowMaps();

			renderer->RenderPass(primaryView, shadowMap);
			guiRenderer->Render(primaryView);
		}

		renderer->timer->EndFrame();
		renderer->EndFrame();

		double frameEnd = glfwGetTime();
		fpsTimer += frameEnd - lastFrameEnd;
		frameCounter++;

		if (fpsTimer > 1.0)
		{
			renderer->SetTitle("STRAY PHOTONS (" + std::to_string(frameCounter) + " FPS)");
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
