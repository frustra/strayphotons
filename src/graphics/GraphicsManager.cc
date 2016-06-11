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

		context = new Renderer(game);
		context->CreateWindow();
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

		RenderArgs renderArgs;
		renderArgs.view = getPlayerViewTransform();
		renderArgs.projection = getPlayerProjectTransform();
		context->RenderFrame(renderArgs);

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
	void GraphicsManager::SetPlayerView(sp::Entity entity)
	{
		validateView(entity);
		playerView = entity;
	}

	glm::mat4 GraphicsManager::getPlayerViewTransform()
	{
		validateView(playerView);

		auto *transform = playerView.Get<ECS::Transform>();
		return glm::inverse(transform->GetModelTransform(*playerView.GetManager()));
	}

	glm::mat4 GraphicsManager::getPlayerProjectTransform()
	{
		validateView(playerView);

		auto *view = playerView.Get<ECS::View>();
		return glm::perspective(view->fov, 1.778f, 0.1f, 256.0f);
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
