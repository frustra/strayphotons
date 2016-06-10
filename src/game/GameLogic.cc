#include "core/Game.hh"
#include "core/Logging.hh"

#include "game/GameLogic.hh"
#include "assets/Model.hh"
#include "ecs/components/Renderable.hh"
#include "ecs/components/Transform.hh"
#include "ecs/components/Physics.hh"

#include <glm/glm.hpp>
#include <cmath>

namespace sp
{
	GameLogic::GameLogic(Game *game) : game(game)
	{
	}

	void GameLogic::Init()
	{
		this->duck = game->entityManager.NewEntity();
		this->duck.Assign<ECS::Renderable>(game->assets.LoadModel("duck"));
		this->duck.Assign<ECS::Physics>(game->physics.CreateActor());
		ECS::Transform *duckTransform = this->duck.Assign<ECS::Transform>();
		duckTransform->Translate(glm::vec3(0, 0, -4));
		duckTransform->Rotate(glm::radians(-90.0f), glm::vec3(0, 1, 0));


		this->box = game->entityManager.NewEntity();
		this->box.Assign<ECS::Renderable>(game->assets.LoadModel("box"));
		ECS::Transform *boxTransform = this->box.Assign<ECS::Transform>();
		boxTransform->Translate(glm::vec3(0, 2, 0));

		this->sponza = game->entityManager.NewEntity();
		this->sponza.Assign<ECS::Renderable>(game->assets.LoadModel("sponza"));
		ECS::Transform *mapTransform = this->sponza.Assign<ECS::Transform>();
		mapTransform->Scale(glm::vec3(1.0f) / 100.0f);
		mapTransform->Translate(glm::vec3(0, -1, 0));
		mapTransform->Rotate(glm::radians(-90.0f), glm::vec3(0, 1, 0));
	}

	GameLogic::~GameLogic()
	{
	}

	bool GameLogic::Frame(double dtSinceLastFrame)
	{
		ECS::Transform *boxTransform = this->box.Get<ECS::Transform>();
		boxTransform->Rotate(3 * dtSinceLastFrame, glm::vec3(0, 1, 0));

		return true;
	}
}

