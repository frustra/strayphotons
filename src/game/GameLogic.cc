#define _USE_MATH_DEFINES
#include <cmath>

#include "core/Game.hh"
#include "core/Logging.hh"
#include "core/CVar.hh"
#include "core/Console.hh"

#include "game/GameLogic.hh"
#include "assets/Scene.hh"
#include "assets/Script.hh"
#include "assets/AssetManager.hh"
#include "ecs/components/Barrier.hh"
#include "ecs/components/Light.hh"
#include "ecs/components/LightSensor.hh"
#include "ecs/components/Physics.hh"
#include "ecs/components/Renderable.hh"
#include "ecs/components/Transform.hh"
#include "ecs/components/Triggerable.h"
#include "ecs/components/TriggerArea.hh"
#include "ecs/components/View.hh"
#include "ecs/components/Controller.hh"
#include "ecs/components/SlideDoor.hh"
#include "ecs/components/VoxelInfo.hh"
#include "ecs/components/SignalReceiver.hh"
#include "ecs/components/Interact.hh"
#include "ecs/components/XRView.hh"
#include "physx/PhysxUtils.hh"
#include "xr/XrSystemFactory.hh"

#include <cxxopts.hpp>
#include <glm/glm.hpp>

namespace sp
{
	GameLogic::GameLogic(Game *game)
		:
		game(game),
		humanControlSystem(&game->entityManager, &game->physics),
		lightGunSystem(&game->entityManager, &game->physics, this),
		doorSystem(game->entityManager),
		sunPos(0),
		gameXrActions("game_actions", "The actions a user can take while playing the game")
	{
		funcs.Register(this, "loadscene", "Load a scene", &GameLogic::LoadScene);
		funcs.Register(this, "reloadscene", "Reload current scene", &GameLogic::ReloadScene);
		funcs.Register(this, "printdebug", "Print some debug info about the scene", &GameLogic::PrintDebug);

		funcs.Register(this, "g.OpenBarrier", "Open barrier by name", &GameLogic::OpenBarrier);
		funcs.Register(this, "g.CloseBarrier", "Close barrier by name", &GameLogic::CloseBarrier);
		funcs.Register(this, "g.OpenDoor", "Open door by name", &GameLogic::OpenDoor);
		funcs.Register(this, "g.CloseDoor", "Open door by name", &GameLogic::CloseDoor);

		InitXrActions();
	}

	static CVar<float> CVarFlashlight("r.Flashlight", 100, "Flashlight intensity");
	static CVar<bool> CVarFlashlightOn("r.FlashlightOn", false, "Flashlight on/off");
	static CVar<string> CVarFlashlightParent("r.FlashlightParent", "player", "Flashlight parent entity name");
	static CVar<float> CVarFlashlightAngle("r.FlashlightAngle", 20, "Flashlight spot angle");
	static CVar<int> CVarFlashlightResolution("r.FlashlightResolution", 512, "Flashlight shadow map resolution");
	static CVar<float> CVarSunPosition("g.SunPosition", 0.2, "Sun angle");

	static CVar<bool> CVarConnectVR("r.ConnectVR", false, "Connect to SteamVR");

	void GameLogic::InitXrActions()
	{
		// Create teleport action
		std::shared_ptr<xr::XrAction<xr::XrActionType::Bool>> teleportAction(new xr::XrAction<xr::XrActionType::Bool>("teleport"));

		// Suggested bindings for Oculus Touch
		teleportAction->AddSuggestedBinding("/interaction_profiles/oculus/touch_controller", "/user/hand/right/input/a/click");

		// Suggested bindings for Valve Index
		teleportAction->AddSuggestedBinding("/interaction_profiles/valve/index_controller", "/user/hand/right/input/trigger/click");

		// Suggested bindings for HTC Vive
		teleportAction->AddSuggestedBinding("/interaction_profiles/htc/vive_controller", "/user/hand/right/input/trackpad/click");

		// Tell the XR runtime we will want to be able to query this action on a per-hand basis
		teleportAction->AddSubactionPath("/user/hand/right");

		gameXrActions.AddAction(teleportAction);

		// Create grab / interract action
		std::shared_ptr<xr::XrAction<xr::XrActionType::Bool>> grabAction(new xr::XrAction<xr::XrActionType::Bool>("grab"));

		// Suggested bindings for Oculus Touch
		grabAction->AddSuggestedBinding("/interaction_profiles/oculus/touch_controller", "/user/hand/left/input/squeeze/value");
		grabAction->AddSuggestedBinding("/interaction_profiles/oculus/touch_controller", "/user/hand/right/input/squeeze/value");

		// Suggested bindings for Valve Index
		grabAction->AddSuggestedBinding("/interaction_profiles/valve/index_controller", "/user/hand/left/input/squeeze/click");
		grabAction->AddSuggestedBinding("/interaction_profiles/valve/index_controller", "/user/hand/right/input/squeeze/click");

		// Suggested bindings for HTC Vive
		grabAction->AddSuggestedBinding("/interaction_profiles/htc/vive_controller", "/user/hand/left/input/squeeze/click");
		grabAction->AddSuggestedBinding("/interaction_profiles/htc/vive_controller", "/user/hand/right/input/squeeze/click");

		// Tell the XR runtime we will want to be able to query this action on a per-hand basis
		grabAction->AddSubactionPath("/user/hand/left");
		grabAction->AddSubactionPath("/user/hand/right");

		gameXrActions.AddAction(grabAction);
	}

	void GameLogic::Init(InputManager *inputManager, Script *startupScript)
	{
		Assert(input == nullptr, "InputManager can only be bound once.");
		if (inputManager != nullptr)
		{
			input = inputManager;

			humanControlSystem.BindInput(inputManager);
			lightGunSystem.BindInput(inputManager);
		}

		if (game->options.count("map"))
		{
			LoadScene(game->options["map"].as<string>());
		}

		if (startupScript != nullptr)
		{
			startupScript->Exec();
		}
		else if (!game->options.count("map"))
		{
			LoadScene("menu");
		}


		input->BindCommand(INPUT_ACTION_KEYBOARD_BASE + "/f5", "reloadscene");
		input->BindCommand(INPUT_ACTION_KEYBOARD_BASE + "/f6", "reloadscene reset");
		input->BindCommand(INPUT_ACTION_KEYBOARD_BASE + "/f7", "reloadshaders");
		input->BindCommand(INPUT_ACTION_KEYBOARD_BASE + "/f", "toggle r.FlashlightOn");
	}

	GameLogic::~GameLogic()
	{
	}

	void GameLogic::HandleInput()
	{
		if (input->FocusLocked()) return;

		if (input->IsPressed(INPUT_ACTION_KEYBOARD_BASE + "/escape"))
		{
			game->menuGui.OpenPauseMenu();
		}
		else if (input->IsPressed(INPUT_ACTION_KEYBOARD_BASE + "/f1") && CVarConnectVR.Get())
		{
			auto vrOrigin = game->entityManager.EntityWith<ecs::Name>("vr-origin");
			auto player = GetPlayer();
			if (vrOrigin && vrOrigin.Has<ecs::Transform>() && player && player.Has<ecs::Transform>())
			{
				auto vrTransform = vrOrigin.Get<ecs::Transform>();
				auto playerTransform = player.Get<ecs::Transform>();

				vrTransform->SetPosition(playerTransform->GetGlobalPosition(game->entityManager) - glm::vec3(0, ecs::PLAYER_CAPSULE_HEIGHT, 0));
			}
		}
		else if (input->IsPressed(INPUT_ACTION_KEYBOARD_BASE + "/q")) // Spawn dodecahedron
		{
			auto entity = game->entityManager.NewEntity();
			auto model = GAssets.LoadModel("dodecahedron");
			entity.Assign<ecs::Renderable>(model);
			entity.Assign<ecs::Transform>(glm::vec3(0, 5, 0));

			PhysxActorDesc desc;
			desc.transform = physx::PxTransform(physx::PxVec3(0, 5, 0));
			auto actor = game->physics.CreateActor(model, desc, entity);

			if (actor)
			{
				entity.Assign<ecs::Physics>(actor, model, desc);
			}
		}
		else if (input->IsPressed(INPUT_ACTION_KEYBOARD_BASE + "/p")) // Toggle flashlight following player
		{
			if (flashlight.Valid())
			{
				auto transform = flashlight.Get<ecs::Transform>();
				auto player = game->entityManager.EntityWith<ecs::Name>(CVarFlashlightParent.Get());
				if (player && player.Has<ecs::Transform>())
				{
					auto playerTransform = player.Get<ecs::Transform>();
					if (transform->HasParent(game->entityManager))
					{
						transform->SetPosition(transform->GetGlobalTransform(game->entityManager) * glm::vec4(0, 0, 0, 1));
						transform->SetRotate(playerTransform->GetGlobalRotation(game->entityManager));
						transform->SetParent(ecs::Entity());
					}
					else
					{
						transform->SetPosition(glm::vec3(0, -0.3, 0));
						transform->SetRotate(glm::quat());
						transform->SetParent(player);
					}
				}
			}
		}
	}

	bool GameLogic::Frame(double dtSinceLastFrame)
	{
		if (input != nullptr)
		{
			HandleInput();
		}

		if (!scene) return true;
		ecs::Entity player = GetPlayer();
		if (!player.Valid()) return true;

		for (auto entity : game->entityManager.EntitiesWith<ecs::TriggerArea>())
		{
			auto area = entity.Get<ecs::TriggerArea>();

			for (auto triggerableEntity : game->entityManager.EntitiesWith<ecs::Triggerable>())
			{
				auto transform = triggerableEntity.Get<ecs::Transform>();
				auto entityPos = transform->GetPosition();
				if (
					entityPos.x > area->boundsMin.x &&
					entityPos.y > area->boundsMin.y &&
					entityPos.z > area->boundsMin.z &&
					entityPos.x < area->boundsMax.x &&
					entityPos.y < area->boundsMax.y &&
					entityPos.z < area->boundsMax.z &&
					!area->triggered
				)
				{
					area->triggered = true;
					Debugf("Entity at: %f %f %f", entityPos.x, entityPos.y, entityPos.z);
					Logf("Triggering event: %s", area->command);
					GConsoleManager.ParseAndExecute(area->command);
				}
			}
		}

		if (!scene) return true;

		ecs::Entity sun = game->entityManager.EntityWith<ecs::Name>("sun");
		if (sun.Valid())
		{
			if (CVarSunPosition.Get() == 0)
			{
				sunPos += dtSinceLastFrame * (0.05 + std::abs(sin(sunPos) * 0.1));
				if (sunPos > M_PI / 2.0) sunPos = -M_PI / 2.0;
			}
			else
			{
				sunPos = CVarSunPosition.Get();
			}

			auto transform = sun.Get<ecs::Transform>();
			transform->SetRotate(glm::mat4());
			transform->Rotate(glm::radians(-90.0), glm::vec3(1, 0, 0));
			transform->Rotate(sunPos, glm::vec3(0, 1, 0));
			transform->SetPosition(glm::vec3(sin(sunPos) * 40.0, cos(sunPos) * 40.0, 0));
		}

		if (CVarFlashlight.Changed())
		{
			auto light = flashlight.Get<ecs::Light>();
			light->intensity = CVarFlashlight.Get(true);
		}
		if (CVarFlashlightOn.Changed())
		{
			auto light = flashlight.Get<ecs::Light>();
			light->on = CVarFlashlightOn.Get(true);
		}
		if (CVarFlashlightAngle.Changed())
		{
			auto light = flashlight.Get<ecs::Light>();
			light->spotAngle = glm::radians(CVarFlashlightAngle.Get(true));
		}
		if (CVarFlashlightResolution.Changed())
		{
			auto view = flashlight.Get<ecs::View>();
			view->SetProjMat(view->GetFov(), view->GetClip(), glm::ivec2(CVarFlashlightResolution.Get(true)));
		}

		if (xrSystem)
		{
			ecs::Entity vrOrigin = game->entityManager.EntityWith<ecs::Name>("vr-origin");

			if (vrOrigin.Valid())
			{
				auto vrOriginTransform = vrOrigin.Get<ecs::Transform>();

				xrSystem->SyncActions(gameXrActions);

				for (auto trackedObjectHandle : xrSystem->GetTrackedObjectHandles())
				{
					ecs::Entity xrObject = ValidateAndLoadTrackedObject(trackedObjectHandle);

					if (xrObject.Valid())
					{
						glm::mat4 xrObjectPos;

						if (xrSystem->GetTracking()->GetPredictedObjectPose(trackedObjectHandle, xrObjectPos))
						{
							xrObjectPos = glm::transpose(xrObjectPos * glm::transpose(vrOriginTransform->GetGlobalTransform(game->entityManager)));

							auto ctrl = xrObject.Get<ecs::Transform>();
							ctrl->SetPosition(xrObjectPos * glm::vec4(0, 0, 0, 1));
							ctrl->SetRotate(glm::mat4(glm::mat3(xrObjectPos)));

							if (trackedObjectHandle.type == xr::TrackedObjectType::CONTROLLER)
							{
								std::string subpath = "";

								if (trackedObjectHandle.hand == xr::TrackedObjectHand::LEFT)
								{
									subpath = "/user/hand/left";
								}
								else if (trackedObjectHandle.hand == xr::TrackedObjectHand::RIGHT)
								{
									subpath = "/user/hand/right";
								}

								xrSystem->GetActionState("teleport", gameXrActions, subpath);
								if (gameXrActions.GetRisingEdgeActionValue("teleport", subpath))
								{
									Logf("Teleport on subpath %s", subpath);

									auto origin = GlmVec3ToPxVec3(ctrl->GetPosition());
									auto dir = GlmVec3ToPxVec3(ctrl->GetForward());
									dir.normalizeSafe();
									physx::PxReal maxDistance = 10.0f;

									physx::PxRaycastBuffer hit;
									bool status = game->physics.RaycastQuery(xrObject, origin, dir, maxDistance, hit);

									if (status && hit.block.distance > 0.5)
									{
										auto headPos = glm::vec3(xrObjectPos * glm::vec4(0, 0, 0, 1)) - vrOriginTransform->GetPosition();
										auto newPos = PxVec3ToGlmVec3P(origin + dir * std::max(0.0, hit.block.distance - 0.5)) - headPos;
										vrOriginTransform->SetPosition(glm::vec3(newPos.x, vrOriginTransform->GetPosition().y, newPos.z));
									}
								}

								auto interact = xrObject.Get<ecs::InteractController>();

								xrSystem->GetActionState("grab", gameXrActions, subpath);
								if (gameXrActions.GetRisingEdgeActionValue("grab", subpath))
								{
									Logf("grab on subpath %s", subpath);
									interact->PickUpObject(xrObject);
								}
								else if (gameXrActions.GetFallingEdgeActionValue("grab", subpath))
								{
									Logf("Let go on subpath %s", subpath);
									if (interact->target)
									{
										interact->manager->RemoveConstraint(xrObject, interact->target);
										interact->target = nullptr;
									}
								}
							}
						}
					}
				}
			}
		}

		if (!humanControlSystem.Frame(dtSinceLastFrame)) return false;
		if (!lightGunSystem.Frame(dtSinceLastFrame)) return false;
		if (!doorSystem.Frame(dtSinceLastFrame)) return false;

		return true;
	}

	ecs::Entity GameLogic::ValidateAndLoadTrackedObject(sp::xr::TrackedObjectHandle &trackedObjectHandle)
	{
		string entityName = trackedObjectHandle.name;
		ecs::Entity xrObject = game->entityManager.EntityWith<ecs::Name>(entityName);

		if (trackedObjectHandle.connected)
		{
			// Make sure object is valid
			if (!xrObject.Valid())
			{
				xrObject = game->entityManager.NewEntity();
				xrObject.AssignKey<ecs::Name>(entityName);
			}

			if (!xrObject.Has<ecs::Transform>())
			{
				xrObject.Assign<ecs::Transform>();
			}

			if (!xrObject.Has<ecs::InteractController>())
			{
				auto interact = xrObject.Assign<ecs::InteractController>();
				interact->manager = &game->physics;
			}

			if (!xrObject.Has<ecs::Renderable>())
			{
				auto renderable = xrObject.Assign<ecs::Renderable>();
				renderable->model = xrSystem->GetTrackedObjectModel(trackedObjectHandle);

				// Rendering an XR HMD model from the viewpoint of an XRView is a bad idea
				if (trackedObjectHandle.type == xr::HMD)
				{
					renderable->xrExcluded = true;
				}
			}

			// Mark the XR HMD as being able to activate TriggerAreas
			if (trackedObjectHandle.type == xr::HMD && !xrObject.Has<ecs::Triggerable>())
			{
				xrObject.Assign<ecs::Triggerable>();
			}
		}
		else
		{
			if (xrObject.Valid())
			{
				xrObject.Destroy();
			}
		}

		return xrObject;
	}

	void GameLogic::LoadScene(string name)
	{
		game->graphics.RenderLoading();
		game->physics.StopSimulation();
		game->entityManager.DestroyAll();

		if (scene != nullptr)
		{
			for (auto &line : scene->unloadExecList)
			{
				GConsoleManager.ParseAndExecute(line);
			}
		}

		scene.reset();
		scene = GAssets.LoadScene(name, &game->entityManager, game->physics);
		if (!scene)
		{
			game->physics.StartSimulation();
			return;
		}

		ecs::Entity player = GetPlayer();
		humanControlSystem.AssignController(player, game->physics);
		player.Assign<ecs::VoxelInfo>();

		// Mark the player as being able to activate trigger areas
		player.Assign<ecs::Triggerable>();

		vector<ecs::Entity> viewEntities;
		viewEntities.push_back(player);

		// On scene load, check if the status of xrSystem matches the status of CVarConnectVR
		if (CVarConnectVR.Get())
		{
			// No xrSystem loaded but CVarConnectVR indicates we should try to load one.
			if (!xrSystem)
			{
				// TODO: refactor this so that xrSystemFactory.GetBestXrSystem() returns a TypeTrait
				xr::XrSystemFactory xrSystemFactory;
				xrSystem = xrSystemFactory.GetBestXrSystem();

				if (xrSystem)
				{
					try
					{
						xrSystem->Init();
					}
					catch (std::exception e)
					{
						Errorf("XR Runtime threw error on initialization! Error: %s", e.what());
						xrSystem.reset();
					}
				}
				else
				{
					Logf("Failed to load an XR runtime");
				}
			}

			// The previous step might have failed to load an XR system, test it is loaded
			if (xrSystem)
			{
				// Create a VR origin if one does not already exist
				ecs::Entity vrOrigin = game->entityManager.EntityWith<ecs::Name>("vr-origin");
				if (!vrOrigin.Valid())
				{
					vrOrigin = game->entityManager.NewEntity();
					// Use AssignKey so we can find this entity by name later
					vrOrigin.AssignKey<ecs::Name>("vr-origin");
				}

				// Add a transform to the VR origin if one does not already exist
				if (!vrOrigin.Has<ecs::Transform>())
				{
					auto transform = vrOrigin.Assign<ecs::Transform>();
					if (player.Valid() && player.Has<ecs::Transform>())
					{
						auto playerTransform = player.Get<ecs::Transform>();
						transform->SetPosition(playerTransform->GetGlobalPosition(game->entityManager) - glm::vec3(0, ecs::PLAYER_CAPSULE_HEIGHT, 0));
						transform->SetRotate(playerTransform->GetRotate());
					}
				}

				// Query the XR runtime for all tracked objects and create entities for them
				for (auto trackedObjectHandle : xrSystem->GetTrackedObjectHandles())
				{
					ValidateAndLoadTrackedObject(trackedObjectHandle);
				}

				// Create swapchains for all the views this runtime exposes. Only create the minimum number of views, since
				// it's unlikely we can render more than the bare minimum. Ex: we don't support 3rd eye rendering for mixed
				// reality capture
				// TODO: add a CVar to allow 3rd eye rendering
				for (unsigned int i = 0; i < xrSystem->GetCompositor()->GetNumViews(true /* minimum */); i++)
				{
					ecs::Entity viewEntity = game->entityManager.NewEntity();
					auto ecsView = viewEntity.Assign<ecs::View>();
					xrSystem->GetCompositor()->PopulateView(i, ecsView);

					// Mark this as an XR View
					auto ecsXrView = viewEntity.Assign<ecs::XRView>();
					ecsXrView->viewId = i;

					viewEntities.push_back(viewEntity);
				}
			}
		}
		// CVar says XR should be disabled. Ensure xrSystem state matches.
		else if (xrSystem)
		{
			xrSystem->Deinit();
			xrSystem.reset();
		}

		game->graphics.SetPlayerView(viewEntities);

		for (auto &line : scene->autoExecList)
		{
			GConsoleManager.ParseAndExecute(line);
		}

		// Create flashlight entity
		flashlight = game->entityManager.NewEntity();
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

	void GameLogic::ReloadScene(string arg)
	{
		if (scene)
		{
			auto player = GetPlayer();
			if (arg == "reset")
			{
				LoadScene(scene->name);
			}
			else if (player && player.Has<ecs::Transform>())
			{
				// Store the player position and set it back on the new player entity
				auto transform = player.Get<ecs::Transform>();
				auto position = transform->GetPosition();
				auto rotation = transform->GetRotate();

				LoadScene(scene->name);

				if (scene)
				{
					player = GetPlayer();
					if (player && player.Has<ecs::HumanController>())
					{
						humanControlSystem.Teleport(player, position, rotation);
					}
				}
			}
		}
	}

	string entityName(ecs::Entity ent)
	{
		string name = ent.ToString();

		if (ent.Has<ecs::Name>())
		{
			name += " (" + *ent.Get<ecs::Name>() + ")";
		}
		return name;
	}

	void GameLogic::PrintDebug()
	{
		Logf("Currently loaded scene: %s", scene ? scene->name : "none");
		if (!scene) return;
		auto player = GetPlayer();
		if (player && player.Has<ecs::Transform>() && player.Has<ecs::HumanController>())
		{
			auto transform = player.Get<ecs::Transform>();
			auto controller = player.Get<ecs::HumanController>();
			auto position = transform->GetPosition();
			auto pxFeet = controller->pxController->getFootPosition();
			Logf("Player position: [%f, %f, %f], feet: %f", position.x, position.y, position.z, pxFeet.y);
			Logf("Player velocity: [%f, %f, %f]", controller->velocity.x, controller->velocity.y, controller->velocity.z);
			Logf("Player on ground: %s", controller->onGround ? "true" : "false");
		}
		else
		{
			Logf("Scene has no valid player");
		}

		for (auto ent : game->entityManager.EntitiesWith<ecs::LightSensor>())
		{
			auto sensor = ent.Get<ecs::LightSensor>();
			auto i = sensor->illuminance;
			string name = entityName(ent);

			Logf("Light sensor %s: %f %f %f", name, i.r, i.g, i.b);
		}

		for (auto ent : game->entityManager.EntitiesWith<ecs::SignalReceiver>())
		{
			auto receiver = ent.Get<ecs::SignalReceiver>();
			string name = entityName(ent);

			Logf("Signal receiver %s: %.2f", name, receiver->GetSignal());
		}
	}

	shared_ptr<xr::XrSystem> GameLogic::GetXrSystem()
	{
		return xrSystem;
	}

	ecs::Entity GameLogic::GetPlayer()
	{
		return game->entityManager.EntityWith<ecs::Name>("player");
	}

	void GameLogic::OpenBarrier(string name)
	{
		auto ent = game->entityManager.EntityWith<ecs::Name>(name);
		if (!ent.Valid())
		{
			return Logf("%s not found", name);
		}

		if (!ent.Has<ecs::Barrier>())
		{
			return Logf("%s is not a barrier", name);
		}

		ecs::Barrier::Open(ent, game->physics);
	}

	void GameLogic::CloseBarrier(string name)
	{
		auto ent = game->entityManager.EntityWith<ecs::Name>(name);
		if (!ent.Valid())
		{
			return Logf("%s not found", name);
		}

		if (!ent.Has<ecs::Barrier>())
		{
			return Logf("%s is not a barrier", name);
		}

		ecs::Barrier::Close(ent, game->physics);
	}

	void GameLogic::OpenDoor(string name)
	{
		auto ent = game->entityManager.EntityWith<ecs::Name>(name);
		if (!ent.Valid())
		{
			return Logf("%s not found", name);
		}

		if (!ent.Has<ecs::SlideDoor>())
		{
			return Logf("%s is not a door", name);
		}

		if (ent.Has<ecs::SignalReceiver>())
		{
			ent.Get<ecs::SignalReceiver>()->SetOffset(1.0f);
		}
		else
		{
			ent.Get<ecs::SlideDoor>()->Open(game->entityManager);
		}
	}

	void GameLogic::CloseDoor(string name)
	{
		auto ent = game->entityManager.EntityWith<ecs::Name>(name);
		if (!ent.Valid())
		{
			return Logf("%s not found", name);
		}

		if (!ent.Has<ecs::SlideDoor>())
		{
			return Logf("%s is not a door", name);
		}

		if (ent.Has<ecs::SignalReceiver>())
		{
			ent.Get<ecs::SignalReceiver>()->SetOffset(-1.0f);
		}
		else
		{
			ent.Get<ecs::SlideDoor>()->Close(game->entityManager);
		}
	}
}
