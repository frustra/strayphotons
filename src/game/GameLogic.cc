#define _USE_MATH_DEFINES
#include <cmath>

#include "core/Game.hh"
#include "core/Logging.hh"
#include "core/CVar.hh"

#include "game/GameLogic.hh"
#include "assets/Scene.hh"
#include "assets/AssetManager.hh"
#include "ecs/components/Renderable.hh"
#include "ecs/components/Transform.hh"
#include "ecs/components/Physics.hh"
#include "ecs/components/View.hh"
#include "ecs/components/Light.hh"

#include <cxxopts.hpp>
#include <glm/glm.hpp>

namespace sp
{
	GameLogic::GameLogic(Game *game)
		: game(game), input(&game->input), humanControlSystem(&game->entityManager, &game->input), flashlightFixed(false), sunPos(0), funcs(this)
	{
		funcs.Register("loadscene", "Load a new scene and replace the existing one", &GameLogic::LoadScene);
	}

	static CVar<float> CVarFlashlight("r.Flashlight", 100, "Flashlight intensity");
	static CVar<float> CVarFlashlightAngle("r.FlashlightAngle", 20, "Flashlight spot angle");
	static CVar<int> CVarFlashlightResolution("r.FlashlightResolution", 512, "Flashlight shadow map resolution");
	static CVar<float> CVarSunPostion("g.SunPostion", 0.2, "Sun angle");

	void GameLogic::Init()
	{
		if (game->options["map"].count())
		{
			LoadScene(game->options["map"].as<string>());
		}

		input->AddCharInputCallback([&](uint32 ch)
		{
			if (input->FocusLocked()) return;
			if (ch == 'q') // Spawn dodecahedron
			{
				auto entity = game->entityManager.NewEntity();
				auto model = GAssets.LoadModel("dodecahedron");
				entity.Assign<ecs::Renderable>(model);
				auto transform = entity.Assign<ecs::Transform>();
				transform->Translate(glm::vec3(0, 5, 0));

				PhysxManager::ActorDesc desc;
				desc.transform = physx::PxTransform(physx::PxVec3(0, 5, 0));
				auto actor = game->physics.CreateActor(model, desc);

				if (actor)
				{
					auto physics = entity.Assign<ecs::Physics>(actor, model);
				}
			}
			else if (ch == 'e') // Toggle flashlight following player
			{
				auto transform = flashlight.Get<ecs::Transform>();
				ecs::Entity player = scene->FindEntity("player");
				auto playerTransform = player.Get<ecs::Transform>();
				flashlightFixed = !flashlightFixed;
				if (flashlightFixed)
				{
					transform->SetTransform(transform->GetModelTransform(game->entityManager));
					transform->SetRelativeTo(ecs::Entity());
				}
				else
				{
					transform->SetTransform(glm::mat4());
					transform->Translate(glm::vec3(0, -0.3, 0));
					transform->SetRelativeTo(player);
				}
			}
			else if (ch == 'f') // Turn flashlight on and off
			{
				auto light = flashlight.Get<ecs::Light>();
				if (!light->intensity)
				{
					light->intensity = CVarFlashlight.Get(true);
				}
				else
				{
					light->intensity = 0;
				}
			}
		});

		//game->audio.StartEvent("event:/german nonsense");
	}

	GameLogic::~GameLogic()
	{
	}

	bool GameLogic::Frame(double dtSinceLastFrame)
	{
		if (!scene) return true;

		ecs::Entity sun = scene->FindEntity("sun");
		if (sun.Valid())
		{
			if (CVarSunPostion.Get() == 0)
			{
				sunPos += dtSinceLastFrame * (0.05 + abs(sin(sunPos) * 0.1));
				if (sunPos > M_PI / 2.0) sunPos = -M_PI / 2.0;
			}
			else
			{
				sunPos = CVarSunPostion.Get();
			}

			auto transform = sun.Get<ecs::Transform>();
			transform->SetTransform(glm::mat4());
			transform->SetRotation(glm::mat4());
			transform->Rotate(glm::radians(-90.0), glm::vec3(1, 0, 0));
			transform->Rotate(sunPos, glm::vec3(0, 1, 0));
			transform->Translate(glm::vec3(sin(sunPos) * 40.0, cos(sunPos) * 40.0, 0));
		}

		if (CVarFlashlight.Changed())
		{
			auto light = flashlight.Get<ecs::Light>();
			if (light->intensity) light->intensity = CVarFlashlight.Get(true);
		}
		if (CVarFlashlightAngle.Changed())
		{
			auto light = flashlight.Get<ecs::Light>();
			light->spotAngle = glm::radians(CVarFlashlightAngle.Get(true));
		}
		if (CVarFlashlightResolution.Changed())
		{
			auto view = flashlight.Get<ecs::View>();
			view->extents = glm::ivec2(CVarFlashlightResolution.Get(true));
		}
		if (!humanControlSystem.Frame(dtSinceLastFrame))
		{
			return false;
		}
		return true;
	}

	void GameLogic::LoadScene(const string &name)
	{
		game->entityManager.DestroyAll();

		scene = GAssets.LoadScene(name, &game->entityManager, game->physics);
		if (!scene) return;

		ecs::Entity player = scene->FindEntity("player");
		humanControlSystem.AssignController(player, game->physics);

		game->graphics.SetPlayerView(player);

		// Create flashlight entity
		flashlight = game->entityManager.NewEntity();
		auto transform = flashlight.Assign<ecs::Transform>();
		transform->Translate(glm::vec3(0, -0.3, 0));
		transform->SetRelativeTo(player);
		auto light = flashlight.Assign<ecs::Light>();
		light->tint = glm::vec3(1.0);
		light->spotAngle = glm::radians(CVarFlashlightAngle.Get(true));
		auto view = flashlight.Assign<ecs::View>();
		view->extents = glm::vec2(CVarFlashlightResolution.Get());
		view->clip = glm::vec2(0.1, 256);
	}
}
