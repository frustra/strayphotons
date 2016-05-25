#include "core/Game.hh"
#include "core/Logging.hh"

#include "assets/Model.hh"
#include "ecs/components/Renderable.hh"

namespace sp
{
	Game::Game() : graphics(this)
	{
	    duck = entityManager.NewEntity();
		auto duckModel = assets.LoadModel("duck");

	    duck.Assign<ECS::Renderable>(duckModel);
		graphics.CreateContext();
	}

	Game::~Game()
	{
	}

	void Game::Start()
	{
		try
		{
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
		if (!graphics.Frame()) return false;

		return true;
	}

	bool Game::ShouldStop()
	{
		return !graphics.HasActiveContext();
	}
}

