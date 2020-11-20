#define _USE_MATH_DEFINES
#include "game/GameLogic.hh"

#include "assets/AssetManager.hh"
#include "assets/Scene.hh"
#include "assets/Script.hh"
#include "core/CVar.hh"
#include "core/Console.hh"
#include "core/Game.hh"
#include "core/Logging.hh"
#include "physx/PhysxUtils.hh"
#include "xr/XrSystemFactory.hh"

#include <cmath>
#include <cxxopts.hpp>
#include <ecs/EcsImpl.hh>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

namespace sp {
	GameLogic::GameLogic(Game *game)
		: game(game), input(&game->input), humanControlSystem(&game->entityManager, &game->input, &game->physics),
		  lightGunSystem(&game->entityManager, &game->input, &game->physics, this), doorSystem(game->entityManager),
		  sunPos(0) {
		funcs.Register(this, "loadscene", "Load a scene", &GameLogic::LoadScene);
		funcs.Register(this, "reloadscene", "Reload current scene", &GameLogic::ReloadScene);
		funcs.Register(this, "printdebug", "Print some debug info about the scene", &GameLogic::PrintDebug);

		funcs.Register(this, "g.OpenBarrier", "Open barrier by name", &GameLogic::OpenBarrier);
		funcs.Register(this, "g.CloseBarrier", "Close barrier by name", &GameLogic::CloseBarrier);
		funcs.Register(this, "g.OpenDoor", "Open door by name", &GameLogic::OpenDoor);
		funcs.Register(this, "g.CloseDoor", "Open door by name", &GameLogic::CloseDoor);
	}

	static CVar<float> CVarFlashlight("r.Flashlight", 100, "Flashlight intensity");
	static CVar<bool> CVarFlashlightOn("r.FlashlightOn", false, "Flashlight on/off");
	static CVar<string> CVarFlashlightParent("r.FlashlightParent", "player", "Flashlight parent entity name");
	static CVar<float> CVarFlashlightAngle("r.FlashlightAngle", 20, "Flashlight spot angle");
	static CVar<int> CVarFlashlightResolution("r.FlashlightResolution", 512, "Flashlight shadow map resolution");
	static CVar<float> CVarSunPosition("g.SunPosition", 0.2, "Sun angle");

	void GameLogic::Init(Script *startupScript) {
		if (game->options.count("map")) { LoadScene(game->options["map"].as<string>()); }

		if (startupScript != nullptr) {
			startupScript->Exec();
		} else if (!game->options.count("map")) {
			LoadScene("menu");
		}

		if (input != nullptr) {
			input->BindCommand(INPUT_ACTION_SET_VR_ORIGIN, "setvrorigin");
			input->BindCommand(INPUT_ACTION_RELOAD_SCENE, "reloadscene");
			input->BindCommand(INPUT_ACTION_RESET_SCENE, "reloadscene reset");
			input->BindCommand(INPUT_ACTION_RELOAD_SHADERS, "reloadshaders");
			input->BindCommand(INPUT_ACTION_TOGGLE_FLASHLIGH, "toggle r.FlashlightOn");
		}
	}

	GameLogic::~GameLogic() {}

	void GameLogic::HandleInput() {
		if (input->FocusLocked()) return;

		if (game->menuGui && input->IsPressed(INPUT_ACTION_OPEN_MENU)) {
			game->menuGui->OpenPauseMenu();
		} else if (input->IsPressed(INPUT_ACTION_SPAWN_DEBUG)) {
			// Spawn dodecahedron
			auto entity = CreateGameLogicEntity();
			auto model = GAssets.LoadModel("dodecahedron");
			entity.Assign<ecs::Renderable>(model);
			entity.Assign<ecs::Transform>(glm::vec3(0, 5, 0));

			PhysxActorDesc desc;
			desc.transform = physx::PxTransform(physx::PxVec3(0, 5, 0));
			auto actor = game->physics.CreateActor(model, desc, entity);

			if (actor) { entity.Assign<ecs::Physics>(actor, model, desc); }
		} else if (input->IsPressed(INPUT_ACTION_DROP_FLASHLIGH)) {
			// Toggle flashlight following player
			if (flashlight.Valid()) {
				auto transform = flashlight.Get<ecs::Transform>();
				auto player = game->entityManager.EntityWith<ecs::Name>(CVarFlashlightParent.Get());
				if (player && player.Has<ecs::Transform>()) {
					auto playerTransform = player.Get<ecs::Transform>();
					if (transform->HasParent(game->entityManager)) {
						transform->SetPosition(
							transform->GetGlobalTransform(game->entityManager) * glm::vec4(0, 0, 0, 1));
						transform->SetRotate(playerTransform->GetGlobalRotation(game->entityManager));
						transform->SetParent(ecs::Entity());
					} else {
						transform->SetPosition(glm::vec3(0, -0.3, 0));
						transform->SetRotate(glm::quat());
						transform->SetParent(player);
					}
				}
			}
		}
	}

	bool GameLogic::Frame(double dtSinceLastFrame) {
		if (input != nullptr) { HandleInput(); }

		if (!scene) return true;
		ecs::Entity player = GetPlayer();
		if (!player.Valid()) return true;

		for (auto entity : game->entityManager.EntitiesWith<ecs::TriggerArea>()) {
			auto area = entity.Get<ecs::TriggerArea>();

			for (auto triggerableEntity : game->entityManager.EntitiesWith<ecs::Triggerable>()) {
				auto transform = triggerableEntity.Get<ecs::Transform>();
				auto entityPos = transform->GetPosition();
				if (entityPos.x > area->boundsMin.x && entityPos.y > area->boundsMin.y &&
					entityPos.z > area->boundsMin.z && entityPos.x < area->boundsMax.x &&
					entityPos.y < area->boundsMax.y && entityPos.z < area->boundsMax.z && !area->triggered) {
					area->triggered = true;
					Debugf("Entity at: %f %f %f", entityPos.x, entityPos.y, entityPos.z);
					Logf("Triggering event: %s", area->command);
					GetConsoleManager().QueueParseAndExecute(area->command);
				}
			}
		}

		if (!scene) return true;

		ecs::Entity sun = game->entityManager.EntityWith<ecs::Name>("sun");
		if (sun.Valid()) {
			if (CVarSunPosition.Get() == 0) {
				sunPos += dtSinceLastFrame * (0.05 + std::abs(sin(sunPos) * 0.1));
				if (sunPos > M_PI / 2.0) sunPos = -M_PI / 2.0;
			} else {
				sunPos = CVarSunPosition.Get();
			}

			auto transform = sun.Get<ecs::Transform>();
			transform->SetRotate(glm::mat4());
			transform->Rotate(glm::radians(-90.0), glm::vec3(1, 0, 0));
			transform->Rotate(sunPos, glm::vec3(0, 1, 0));
			transform->SetPosition(glm::vec3(sin(sunPos) * 40.0, cos(sunPos) * 40.0, 0));
		}

		if (CVarFlashlight.Changed()) {
			auto light = flashlight.Get<ecs::Light>();
			light->intensity = CVarFlashlight.Get(true);
		}
		if (CVarFlashlightOn.Changed()) {
			auto light = flashlight.Get<ecs::Light>();
			light->on = CVarFlashlightOn.Get(true);
		}
		if (CVarFlashlightAngle.Changed()) {
			auto light = flashlight.Get<ecs::Light>();
			light->spotAngle = glm::radians(CVarFlashlightAngle.Get(true));
		}
		if (CVarFlashlightResolution.Changed()) {
			auto view = flashlight.Get<ecs::View>();
			view->SetProjMat(view->GetFov(), view->GetClip(), glm::ivec2(CVarFlashlightResolution.Get(true)));
		}

		if (!humanControlSystem.Frame(dtSinceLastFrame)) return false;
		if (!lightGunSystem.Frame(dtSinceLastFrame)) return false;
		if (!doorSystem.Frame(dtSinceLastFrame)) return false;

		return true;
	}

	void GameLogic::LoadScene(string name) {
		game->graphics.RenderLoading();
		game->physics.StopSimulation();
		game->entityManager.DestroyAllWith<ecs::Owner>(ecs::OwnerType::GAME_LOGIC);

		if (scene != nullptr) {
			for (auto &line : scene->unloadExecList) {
				GetConsoleManager().ParseAndExecute(line);
			}
		}

		scene.reset();
		scene = GAssets.LoadScene(name, &game->entityManager, game->physics, ecs::OwnerType::GAME_LOGIC);
		if (!scene) {
			game->physics.StartSimulation();
			return;
		}

		ecs::Entity player = GetPlayer();
		humanControlSystem.AssignController(player, game->physics);
		player.Assign<ecs::VoxelInfo>();

		// Mark the player as being able to activate trigger areas
		player.Assign<ecs::Triggerable>();

		game->graphics.AddPlayerView(player);

		for (auto &line : scene->autoExecList) {
			GetConsoleManager().ParseAndExecute(line);
		}

		// Create flashlight entity
		flashlight = CreateGameLogicEntity();
		auto transform = flashlight.Assign<ecs::Transform>(glm::vec3(0, -0.3, 0));
		transform->SetParent(player);
		auto light = flashlight.Assign<ecs::Light>();
		light->tint = glm::vec3(1.0);
		light->spotAngle = glm::radians(CVarFlashlightAngle.Get(true));
		light->intensity = CVarFlashlight.Get(true);
		light->on = CVarFlashlightOn.Get(true);
		auto view = flashlight.Assign<ecs::View>();
		view->fov = light->spotAngle * 2.0f;
		view->extents = glm::vec2(CVarFlashlightResolution.Get(true));
		view->clip = glm::vec2(0.1, 64);

		// Make sure all objects are in the correct physx state before restarting simulation
		game->physics.LogicFrame(game->entityManager);
		game->physics.StartSimulation();
	}

	void GameLogic::ReloadScene(string arg) {
		if (scene) {
			auto player = GetPlayer();
			if (arg == "reset") {
				LoadScene(scene->name);
			} else if (player && player.Has<ecs::Transform>()) {
				// Store the player position and set it back on the new player entity
				auto transform = player.Get<ecs::Transform>();
				auto position = transform->GetPosition();
				auto rotation = transform->GetRotate();

				LoadScene(scene->name);

				if (scene) {
					player = GetPlayer();
					if (player && player.Has<ecs::HumanController>()) {
						humanControlSystem.Teleport(player, position, rotation);
					}
				}
			}
		}
	}

	string entityName(ecs::Entity ent) {
		string name = ent.ToString();

		if (ent.Has<ecs::Name>()) { name += " (" + *ent.Get<ecs::Name>() + ")"; }
		return name;
	}

	void GameLogic::PrintDebug() {
		Logf("Currently loaded scene: %s", scene ? scene->name : "none");
		if (!scene) return;
		auto player = GetPlayer();
		if (player && player.Has<ecs::Transform>() && player.Has<ecs::HumanController>()) {
			auto transform = player.Get<ecs::Transform>();
			auto controller = player.Get<ecs::HumanController>();
			auto position = transform->GetPosition();
			auto pxFeet = controller->pxController->getFootPosition();
			Logf("Player position: [%f, %f, %f], feet: %f", position.x, position.y, position.z, pxFeet.y);
			Logf("Player velocity: [%f, %f, %f]",
				controller->velocity.x,
				controller->velocity.y,
				controller->velocity.z);
			Logf("Player on ground: %s", controller->onGround ? "true" : "false");
		} else {
			Logf("Scene has no valid player");
		}

		for (auto ent : game->entityManager.EntitiesWith<ecs::LightSensor>()) {
			auto sensor = ent.Get<ecs::LightSensor>();
			auto i = sensor->illuminance;
			string name = entityName(ent);

			Logf("Light sensor %s: %f %f %f", name, i.r, i.g, i.b);
		}

		for (auto ent : game->entityManager.EntitiesWith<ecs::SignalReceiver>()) {
			auto receiver = ent.Get<ecs::SignalReceiver>();
			string name = entityName(ent);

			Logf("Signal receiver %s: %.2f", name, receiver->GetSignal());
		}
	}

	ecs::Entity GameLogic::GetPlayer() {
		return game->entityManager.EntityWith<ecs::Name>("player");
	}

	void GameLogic::OpenBarrier(string name) {
		auto ent = game->entityManager.EntityWith<ecs::Name>(name);
		if (!ent.Valid()) { return Logf("%s not found", name); }

		if (!ent.Has<ecs::Barrier>()) { return Logf("%s is not a barrier", name); }

		ecs::Barrier::Open(ent, game->physics);
	}

	void GameLogic::CloseBarrier(string name) {
		auto ent = game->entityManager.EntityWith<ecs::Name>(name);
		if (!ent.Valid()) { return Logf("%s not found", name); }

		if (!ent.Has<ecs::Barrier>()) { return Logf("%s is not a barrier", name); }

		ecs::Barrier::Close(ent, game->physics);
	}

	void GameLogic::OpenDoor(string name) {
		auto ent = game->entityManager.EntityWith<ecs::Name>(name);
		if (!ent.Valid()) { return Logf("%s not found", name); }

		if (!ent.Has<ecs::SlideDoor>()) { return Logf("%s is not a door", name); }

		if (ent.Has<ecs::SignalReceiver>()) {
			ent.Get<ecs::SignalReceiver>()->SetOffset(1.0f);
		} else {
			ent.Get<ecs::SlideDoor>()->Open(game->entityManager);
		}
	}

	void GameLogic::CloseDoor(string name) {
		auto ent = game->entityManager.EntityWith<ecs::Name>(name);
		if (!ent.Valid()) { return Logf("%s not found", name); }

		if (!ent.Has<ecs::SlideDoor>()) { return Logf("%s is not a door", name); }

		if (ent.Has<ecs::SignalReceiver>()) {
			ent.Get<ecs::SignalReceiver>()->SetOffset(-1.0f);
		} else {
			ent.Get<ecs::SlideDoor>()->Close(game->entityManager);
		}
	}

	/**
	 * @brief Helper function used when creating new entities that belong to
	 * 		  GameLogic. Using this function ensures that the correct ecs::Creator
	 * 		  attribute is added to each entity that is owned by GameLogic, and
	 *		  therefore it gets destroyed on scene unload.
	 *
	 * @return ecs::Entity A new Entity with the ecs::Creator::GAME_LOGIC Key added.
	 */
	inline ecs::Entity GameLogic::CreateGameLogicEntity() {
		auto newEntity = game->entityManager.NewEntity();
		newEntity.Assign<ecs::Owner>(ecs::OwnerType::GAME_LOGIC);
		return newEntity;
	}
} // namespace sp
