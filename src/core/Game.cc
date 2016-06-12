#include "core/Game.hh"
#include "core/Logging.hh"
#include "core/Console.hh"

#include "assets/Model.hh"
#include "ecs/components/Renderable.hh"
#include "ecs/components/Transform.hh"
#include "ecs/components/Controller.hh"
#include "ecs/components/View.hh"

#include <glm/glm.hpp>

namespace sp
{
	Game::Game() : graphics(this), logic(this)
	{
		// pre-register all of our component types so that errors do not arise if they
		// are queried for before an instance is ever created
		entityManager.RegisterComponentType<ECS::Renderable>();
		entityManager.RegisterComponentType<ECS::Transform>();
		entityManager.RegisterComponentType<ECS::HumanController>();
		entityManager.RegisterComponentType<ECS::View>();
	}

	Game::~Game()
	{
	}

	void Game::Start()
	{
		try
		{
			logic.Init();
			graphics.CreateContext();
			graphics.BindContextInputCallbacks(input);
			gui.BindInput(input);
			lastFrameTime = glfwGetTime();

			while (true)
			{
				if (ShouldStop()) break;
				if (!Frame()) break;
			}
		}
		catch (char const *err)
		{
			Errorf(err);
		}
	}

	bool Game::Frame()
	{
		double frameTime = glfwGetTime();
		double dt = frameTime - lastFrameTime;

		input.Checkpoint();
		if (input.IsDown(GLFW_KEY_ESCAPE))
		{
			return false;
		}

		GConsoleManager.Update();

		if (!logic.Frame(dt)) return false;
		if (!graphics.Frame()) return false;

		lastFrameTime = frameTime;
		return true;
	}

	bool Game::ShouldStop()
	{
		return !graphics.HasActiveContext();
	}
}

