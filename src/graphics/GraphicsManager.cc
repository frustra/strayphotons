#include "core/Logging.hh"
#include "graphics/GraphicsManager.hh"
#include "graphics/Renderer.hh"
#include "ecs/components/View.hh"
#include "ecs/components/Transform.hh"

#include <iostream>
#include <system_error>

namespace sp
{
	static void glfwErrorCallback(int error, const char *message)
	{
		Errorf("GLFW returned %d: %s", error, message);
	}

	GraphicsManager::GraphicsManager(Game *game) : context(nullptr), game(game)
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
		if (context) ReleaseContext();
		glfwTerminate();
	}

	void GraphicsManager::CreateContext()
	{
		if (context) throw "already an active context";

		auto renderer = new Renderer(game);
		context = renderer;
		context->CreateWindow();

		guiRenderer = new GuiRenderer(renderer);
	}

	void GraphicsManager::ReleaseContext()
	{
		if (!context) throw "no active context";

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
		if (!context) throw "missing context";
		if (!HasActiveContext()) return false;

		auto view = updateViewCaches(playerView);
		context->RenderPass(view);
		guiRenderer->Render(view);

		double frameEnd = glfwGetTime();
		fpsTimer += frameEnd - lastFrameEnd;
		frameCounter++;

		if (fpsTimer > 1.0)
		{
			context->SetTitle("STRAY PHOTONS (" + std::to_string(frameCounter) + " FPS)");
			frameCounter = 0;
			fpsTimer = 0;
		}

		context->EndFrame();
		lastFrameEnd = frameEnd;
		glfwPollEvents();
		return true;
	}

	/**
	* This View will be used when rendering from the player's viewpoint
	*/
	void GraphicsManager::SetPlayerView(Entity entity)
	{
		validateView(entity);
		playerView = entity;
	}

	ECS::View &GraphicsManager::updateViewCaches(Entity entity)
	{
		validateView(entity);

		auto *view = playerView.Get<ECS::View>();

		view->aspect = (float)view->extents.x / (float)view->extents.y;
		view->projMat = glm::perspective(view->fov, view->aspect, view->clip[0], view->clip[1]);
		view->invProjMat = glm::inverse(view->projMat);

		auto *transform = playerView.Get<ECS::Transform>();
		view->invViewMat = transform->GetModelTransform(*playerView.GetManager());
		view->viewMat = glm::inverse(view->invViewMat);

		return *view;
	}

	void GraphicsManager::validateView(Entity viewEntity)
	{
		if (!viewEntity.Valid())
		{
			throw std::runtime_error("view entity is not valid because the entity has been deleted");
		}
		if (!viewEntity.Has<ECS::View>())
		{
			throw std::runtime_error("view entity is not valid because it has no View component");
		}
		if (!viewEntity.Has<ECS::Transform>())
		{
			throw std::runtime_error("view entity is not valid because it has no Transform component");
		}
	}
}
