#include "core/Game.hh"
#include "core/Logging.hh"

#include "game/GameLogic.hh"
#include "assets/Model.hh"
#include "ecs/components/Renderable.hh"
#include "ecs/components/Transform.hh"

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
		this->duck = game->entityManager.NewEntity();
		this->duck.Assign<ECS::Renderable>(game->assets.LoadModel("duck"));
		ECS::Transform *duckTransform = this->duck.Assign<ECS::Transform>();
		duckTransform->Translate(glm::vec3(0, 0, -4));

		this->box = game->entityManager.NewEntity();
		this->box.Assign<ECS::Renderable>(game->assets.LoadModel("box"));
		ECS::Transform *boxTransform = this->box.Assign<ECS::Transform>();
		boxTransform->SetRelativeTo(duck);
		boxTransform->Translate(glm::vec3(0, 2, 0));

		this->sponza = game->entityManager.NewEntity();
		this->sponza.Assign<ECS::Renderable>(game->assets.LoadModel("sponza"));
		ECS::Transform *mapTransform = this->sponza.Assign<ECS::Transform>();
		mapTransform->Scale(glm::vec3(1.0f) / 100.0f);
		mapTransform->Translate(glm::vec3(0, -1, 0));
		mapTransform->Rotate(glm::radians(-90.0f), glm::vec3(0, 1, 0));

		Entity player = game->entityManager.NewEntity();
		auto *playerTransform = player.Assign<ECS::Transform>();
		playerTransform->Translate(glm::vec3(0.0f, 1.0f, 0.0f));
		auto *playerCamera = player.Assign<ECS::Camera>();
		playerCamera->fov = glm::radians(90.0f);
		humanControlSystem.AssignController(player);

		game->cameraSystem.SetActiveCamera(player);
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

