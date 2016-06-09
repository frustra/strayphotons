#include "core/Game.hh"
#include "core/Logging.hh"

#include "assets/Model.hh"
#include "ecs/components/Renderable.hh"
#include "ecs/components/Placement.hh"

#include <glm/glm.hpp>

namespace sp
{
	Game::Game() : graphics(this), logic(this)
	{
		// pre-register all of our component types so that errors do not arrise if they
		// are queried for before an instance is ever created
		entityManager.RegisterComponentType<ECS::Renderable>();
		entityManager.RegisterComponentType<ECS::Placement>();
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
			this->lastFrameTime = glfwGetTime();

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
		double dt = this->lastFrameTime - frameTime;

		if (!logic.Frame(dt)) return false;
		if (!graphics.Frame()) return false;

		this->lastFrameTime = frameTime;
		return true;
	}

	bool Game::ShouldStop()
	{
		return !graphics.HasActiveContext();
	}
}

