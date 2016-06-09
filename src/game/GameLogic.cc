#include "core/Game.hh"
#include "core/Logging.hh"

#include "game/GameLogic.hh"
#include "assets/Model.hh"
#include "ecs/components/Renderable.hh"
#include "ecs/components/Placement.hh"

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
		ECS::Placement *duckPlacement = this->duck.Assign<ECS::Placement>();
		duckPlacement->Translate(glm::vec3(0, 0, -4));
		duckPlacement->Rotate(glm::radians(-30.0f), glm::vec3(0, 0, 1));

		this->box = game->entityManager.NewEntity();
		this->box.Assign<ECS::Renderable>(game->assets.LoadModel("box"));
		ECS::Placement *boxPlacement = this->box.Assign<ECS::Placement>();
		boxPlacement->SetRelativeTo(duck);
		boxPlacement->Translate(glm::vec3(0, 2, 0));

		this->sponza = game->entityManager.NewEntity();
		this->sponza.Assign<ECS::Renderable>(game->assets.LoadModel("sponza"));
		ECS::Placement *mapPlacement = this->sponza.Assign<ECS::Placement>();
		mapPlacement->Scale(glm::vec3(1.0f) / 100.0f);
		mapPlacement->Translate(glm::vec3(0, -1, 0));
		mapPlacement->Rotate(glm::radians(-90.0f), glm::vec3(0, 1, 0));
	}

	GameLogic::~GameLogic()
	{
	}

	bool GameLogic::Frame(double dtSinceLastFrame)
	{
		ECS::Placement *boxPlacement = this->box.Get<ECS::Placement>();
		boxPlacement->Rotate(3 * dtSinceLastFrame, glm::vec3(0, 1, 0));

		auto *duckPlacement = this->duck.Get<ECS::Placement>();
		duckPlacement->Rotate(dtSinceLastFrame, glm::vec3(1, 0, 0));

		return true;
	}
}

