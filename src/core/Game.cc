#include "core/Game.hh"
#include "core/Logging.hh"

#include <iostream>

namespace sp
{
	Game::Game()
	{
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

