#include "core/Game.hh"
#include "core/Logging.hh"

#include "game/GameLogic.hh"
#include "assets/Scene.hh"
#include "assets/AssetManager.hh"
#include "ecs/components/Renderable.hh"
#include "ecs/components/Transform.hh"
#include "ecs/components/Physics.hh"
#include "ecs/components/View.hh"

#include <glm/glm.hpp>
#include <cmath>

namespace sp
{
	GameLogic::GameLogic(Game *game)
		: game(game), humanControlSystem(&game->entityManager, &game->input)
	{
	}

	void GameLogic::Init()
	{
		scene = GAssets.LoadScene("sponza", &game->entityManager, game->physics);

		ecs::Entity player = scene->FindEntity("player");
		humanControlSystem.AssignController(player);

		game->graphics.SetPlayerView(player);
	}

	GameLogic::~GameLogic()
	{
	}

	bool GameLogic::Frame(double dtSinceLastFrame)
	{
		if (!humanControlSystem.Frame(dtSinceLastFrame))
		{
			return false;
		}
		return true;
	}
}

