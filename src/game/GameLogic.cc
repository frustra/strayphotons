#include "core/Game.hh"
#include "core/Logging.hh"

#include "game/GameLogic.hh"
#include "assets/Model.hh"
#include "ecs/components/Renderable.hh"
#include "ecs/components/Transform.hh"
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
		auto entities = game->assets.LoadScene("sponza", &game->entityManager);
		// TODO(xthexder): Make a proper lookup function
		duck = entities[1];
		box = entities[2];

		Entity player = entities[3];
		humanControlSystem.AssignController(player);

		game->graphics.SetPlayerView(player);
	}

	GameLogic::~GameLogic()
	{
	}

	bool GameLogic::Frame(double dtSinceLastFrame)
	{
		ECS::Transform *boxTransform = this->box.Get<ECS::Transform>();
		boxTransform->Rotate(3 * dtSinceLastFrame, glm::vec3(0, 1, 0));

		auto *duckTransform = this->duck.Get<ECS::Transform>();
		duckTransform->Rotate(dtSinceLastFrame, glm::vec3(1, 0, 0));

		if (!humanControlSystem.Frame(dtSinceLastFrame))
		{
			return false;
		}

		return true;
	}
}

