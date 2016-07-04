#include "core/Game.hh"
#include "core/Logging.hh"

#include "game/GameLogic.hh"
#include "assets/Scene.hh"
#include "assets/AssetManager.hh"
#include "ecs/components/Renderable.hh"
#include "ecs/components/Transform.hh"
<<<<<<< HEAD
#include "ecs/components/Physics.hh"
=======
#include "ecs/components/View.hh"
>>>>>>> master

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
		scene = GAssets.LoadScene("sponza", &game->entityManager);

		ecs::Entity player = scene->FindEntity("player");
		humanControlSystem.AssignController(player);

		game->graphics.SetPlayerView(player);
	}

	GameLogic::~GameLogic()
	{
	}

	bool GameLogic::Frame(double dtSinceLastFrame)
	{

		auto boxTransform = scene->FindEntity("box").Get<ecs::Transform>();
		boxTransform->Rotate(3.0f * dtSinceLastFrame, glm::vec3(0, 1, 0));

		auto duckTransform = scene->FindEntity("duck").Get<ecs::Transform>();
		duckTransform->Rotate(dtSinceLastFrame, glm::vec3(1, 0, 0));

		if (!humanControlSystem.Frame(dtSinceLastFrame))
		{
			return false;
		}
		return true;
	}
}

