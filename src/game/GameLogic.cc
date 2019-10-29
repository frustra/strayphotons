#define _USE_MATH_DEFINES
#include <cmath>

#include "core/Game.hh"
#include "core/Logging.hh"
#include "core/CVar.hh"
#include "core/Console.hh"

#include "game/GameLogic.hh"
#include "assets/Scene.hh"
#include "assets/AssetManager.hh"
#include "ecs/components/Barrier.hh"
#include "ecs/components/Light.hh"
#include "ecs/components/LightSensor.hh"
#include "ecs/components/Physics.hh"
#include "ecs/components/Renderable.hh"
#include "ecs/components/Transform.hh"
#include "ecs/components/TriggerArea.hh"
#include "ecs/components/View.hh"
#include "ecs/components/Controller.hh"
#include "ecs/components/SlideDoor.hh"
#include "ecs/components/VoxelInfo.hh"
#include "ecs/components/SignalReceiver.hh"
#include "ecs/components/Interact.hh"
#include "physx/PhysxUtils.hh"

#include <cxxopts.hpp>
#include <glm/glm.hpp>

#ifdef ENABLE_VR
#include <openvr.h>
#endif

namespace sp
{
	GameLogic::GameLogic(Game *game)
		:
		game(game),
		input(&game->input),
		humanControlSystem(&game->entityManager, &game->input, &game->physics),
		lightGunSystem(&game->entityManager, &game->input, &game->physics, this),
		doorSystem(game->entityManager),
		sunPos(0)
	{
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
	static CVar<float> CVarFlashlightAngle("r.FlashlightAngle", 20, "Flashlight spot angle");
	static CVar<int> CVarFlashlightResolution("r.FlashlightResolution", 512, "Flashlight shadow map resolution");
	static CVar<float> CVarSunPosition("g.SunPosition", 0.2, "Sun angle");

#ifdef ENABLE_VR
	static CVar<bool> CVarConnectVR("r.ConnectVR", false, "Connect to SteamVR");
#endif

	void GameLogic::Init()
	{
		if (game->options["map"].count())
		{
			LoadScene(game->options["map"].as<string>());
		}
		else
		{
			LoadScene("menu");
		}

		input->BindCommand(GLFW_KEY_F5, "reloadscene");
		input->BindCommand(GLFW_KEY_F6, "reloadscene reset");
		input->BindCommand(GLFW_KEY_F, "toggle r.FlashlightOn");

		input->AddKeyInputCallback([&](int key, int state)
		{
			if (input->FocusLocked()) return;

			if (state == GLFW_PRESS)
			{
				if (key == GLFW_KEY_ESCAPE)
				{
					game->menuGui.OpenPauseMenu();
				}
#ifdef ENABLE_VR
				else if (key == GLFW_KEY_F1 && CVarConnectVR.Get())
				{
					for (auto entity : game->entityManager.EntitiesWith<ecs::Transform, ecs::Name>())
					{
						auto name = entity.Get<ecs::Name>();
						if (name->name == "vr-origin")
						{
							auto transform = entity.Get<ecs::Transform>();
							auto player = scene->FindEntity("player");
							if (player.Valid())
							{
								auto playerTransform = player.Get<ecs::Transform>();
								transform->SetPosition(playerTransform->GetGlobalPosition() - glm::vec3(0, ecs::PLAYER_CAPSULE_HEIGHT, 0));
							}
						}
					}
				}
#endif
				else if (key == GLFW_KEY_Q) // Spawn dodecahedron
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
				else if (key == GLFW_KEY_P) // Toggle flashlight following player
				{
					if (flashlight.Valid())
					{
						auto transform = flashlight.Get<ecs::Transform>();
						ecs::Entity player = scene->FindEntity("player");
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
		});

		input->AddCharInputCallback([&](uint32 ch)
		{
			if (input->FocusLocked()) return;
		});

		//game->audio.StartEvent("event:/german nonsense");
	}

	GameLogic::~GameLogic()
	{
	}

	bool GameLogic::Frame(double dtSinceLastFrame)
	{
		if (!scene) return true;
		ecs::Entity player = scene->FindEntity("player");
		if (!player.Valid()) return true;

		for (auto entity : game->entityManager.EntitiesWith<ecs::TriggerArea>())
		{
			auto area = entity.Get<ecs::TriggerArea>();
			auto transform = player.Get<ecs::Transform>();
			auto playerPos = transform->GetPosition();
			if (
				playerPos.x > area->boundsMin.x &&
				playerPos.y > area->boundsMin.y &&
				playerPos.z > area->boundsMin.z &&
				playerPos.x < area->boundsMax.x &&
				playerPos.y < area->boundsMax.y &&
				playerPos.z < area->boundsMax.z &&
				!area->triggered
			)
			{
				area->triggered = true;
				Debugf("Player at: %f %f %f", playerPos.x, playerPos.y, playerPos.z);
				Logf("Triggering event: %s", area->command);
				GConsoleManager.ParseAndExecute(area->command);
			}
		}

		if (!scene) return true;

		ecs::Entity sun = scene->FindEntity("sun");
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
			view->extents = glm::ivec2(CVarFlashlightResolution.Get(true));
		}

#ifdef ENABLE_VR
		if (vrSystem != nullptr)
		{
			ecs::Entity vrOrigin, vrController;

			for (auto entity : game->entityManager.EntitiesWith<ecs::Name>())
			{
				auto name = entity.Get<ecs::Name>();
				if (name->name == "vr-origin")
					vrOrigin = entity;
				else if (name->name == "vr-controller")
					vrController = entity;
			}

			auto transform = vrOrigin.Get<ecs::Transform>();
			glm::mat4 headTransform;
			vr::TrackedDevicePose_t trackedDevicePoses[vr::k_unMaxTrackedDeviceCount];
			vr::VRCompositor()->WaitGetPoses(trackedDevicePoses, vr::k_unMaxTrackedDeviceCount, NULL, 0);
			if (trackedDevicePoses[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid)
			{
				headTransform = glm::mat4(glm::make_mat3x4((float *)trackedDevicePoses[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking.m));
				headTransform = headTransform * glm::transpose(transform->GetGlobalTransform());
				eyeEntity[0].Get<ecs::View>()->invViewMat = glm::transpose(eyePos[0] * headTransform);
				eyeEntity[1].Get<ecs::View>()->invViewMat = glm::transpose(eyePos[1] * headTransform);
			}

			for (uint32 i = 1; i < vr::k_unMaxTrackedDeviceCount; i++)
			{
				if (trackedDevicePoses[i].bPoseIsValid && vrSystem->GetTrackedDeviceClass(i) == vr::TrackedDeviceClass_Controller)
				{
					auto pos = glm::mat4(glm::make_mat3x4((float *)trackedDevicePoses[i].mDeviceToAbsoluteTracking.m));
					pos = glm::transpose(pos * glm::transpose(transform->GetGlobalTransform()));

					auto ctrl = vrController.Get<ecs::Transform>();
					ctrl->SetPosition(pos * glm::vec4(0, 0, 0, 1));
					ctrl->SetRotate(glm::mat4(glm::mat3(pos)));

					if (!vrController.Has<ecs::Renderable>())
					{
						char modelName[128];
						auto len = vrSystem->GetStringTrackedDeviceProperty(i, vr::Prop_RenderModelName_String, modelName, 128);

						Logf("Loading VR render model %s", modelName);
						vr::RenderModel_t *vrModel;
						vr::RenderModel_TextureMap_t *vrTex;
						vr::EVRRenderModelError merr;

						while (true)
						{
							merr = vr::VRRenderModels()->LoadRenderModel_Async(modelName, &vrModel);
							if (merr != vr::VRRenderModelError_Loading) break;
							std::this_thread::sleep_for(std::chrono::milliseconds(1));
						}
						if (merr != vr::VRRenderModelError_None)
						{
							Errorf("VR render model error: %s", vr::VRRenderModels()->GetRenderModelErrorNameFromEnum(merr));
							throw std::runtime_error("Failed to load VR render model");
						}

						while (true)
						{
							merr = vr::VRRenderModels()->LoadTexture_Async(vrModel->diffuseTextureId, &vrTex);
							if (merr != vr::VRRenderModelError_Loading) break;
							std::this_thread::sleep_for(std::chrono::milliseconds(1));
						}
						if (merr != vr::VRRenderModelError_None)
						{
							Errorf("VR render texture error: %s", vr::VRRenderModels()->GetRenderModelErrorNameFromEnum(merr));
							throw std::runtime_error("Failed to load VR render texture");
						}

						auto r = vrController.Assign<ecs::Renderable>();
						r->model = make_shared<VRModel>(vrModel, vrTex);

						vr::VRRenderModels()->FreeTexture(vrTex);
						vr::VRRenderModels()->FreeRenderModel(vrModel);
					}

					vr::VRControllerState_t state;
					if (vrSystem->GetControllerState(i, &state, sizeof(state)))
					{
						auto interact = vrController.Get<ecs::InteractController>();
						bool touchpadPressed = state.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_Axis0);
						bool triggerPressed = state.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_Axis1);
						if (touchpadPressed && !vrTouchpadPressed)
						{
							Logf("Teleport");

							auto origin = GlmVec3ToPxVec3(ctrl->GetPosition());
							auto dir = GlmVec3ToPxVec3(ctrl->GetForward());
							dir.normalizeSafe();
							physx::PxReal maxDistance = 10.0f;

							physx::PxRaycastBuffer hit;
							bool status = game->physics.RaycastQuery(vrController, origin, dir, maxDistance, hit);
							if (status && hit.block.distance > 0.5)
							{
								auto headPos = glm::vec3(glm::transpose(headTransform) * glm::vec4(0, 0, 0, 1)) - transform->GetPosition();
								auto newPos = PxVec3ToGlmVec3P(origin + dir * std::max(0.0, hit.block.distance - 0.5)) - headPos;
								transform->SetPosition(glm::vec3(newPos.x, transform->GetPosition().y, newPos.z));
							}
						}
						else if (triggerPressed && !vrTriggerPressed)
						{
							Logf("Grab");

							interact->PickUpObject(vrController);
						}
						else if (!triggerPressed && vrTriggerPressed)
						{
							Logf("Let go");
							if (interact->target)
							{
								interact->manager->RemoveConstraint(vrController, interact->target);
								interact->target = nullptr;
							}
						}
						vrTouchpadPressed = touchpadPressed;
						vrTriggerPressed = triggerPressed;
					}

					break;
				}
			}
		}
#endif

		if (!humanControlSystem.Frame(dtSinceLastFrame)) return false;
		if (!lightGunSystem.Frame(dtSinceLastFrame)) return false;
		if (!doorSystem.Frame(dtSinceLastFrame)) return false;

		return true;
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

		ecs::Entity player = scene->FindEntity("player");
		humanControlSystem.AssignController(player, game->physics);
		player.Assign<ecs::VoxelInfo>();

		vector<ecs::Entity> viewEntities;
		viewEntities.push_back(player);

#ifdef ENABLE_VR
		if (CVarConnectVR.Get())
		{
			if (vrSystem == nullptr)
			{
				vr::EVRInitError err = vr::VRInitError_None;
				vrSystem = vr::VR_Init(&err, vr::VRApplication_Scene);
				if (err != vr::VRInitError_None)
				{
					throw std::runtime_error("Failed to init VR system");
				}
			}

			ecs::Entity vrOrigin = scene->FindEntity("vr-origin");
			if (!vrOrigin.Valid())
			{
				vrOrigin = game->entityManager.NewEntity();
				vrOrigin.Assign<ecs::Name>("vr-origin");
			}
			if (!vrOrigin.Has<ecs::Transform>())
			{
				auto transform = vrOrigin.Assign<ecs::Transform>();
				if (player.Valid() && player.Has<ecs::Transform>())
				{
					auto playerTransform = player.Get<ecs::Transform>();
					transform->SetPosition(playerTransform->GetGlobalPosition() - glm::vec3(0, ecs::PLAYER_CAPSULE_HEIGHT, 0));
					transform->SetRotate(playerTransform->GetRotate());
				}
			}

			{
				ecs::Entity vrController = game->entityManager.NewEntity();
				vrController.Assign<ecs::Name>("vr-controller");
				vrController.Assign<ecs::Transform>();
				auto interact = vrController.Assign<ecs::InteractController>();
				interact->manager = &game->physics;
			}

			uint32_t vrWidth, vrHeight;
			vrSystem->GetRecommendedRenderTargetSize(&vrWidth, &vrHeight);

			Debugf("VR headset render size: %dx%d", vrWidth, vrHeight);

			for (int i = 0; i < 2; i++)
			{
				auto eyeType = i == 0 ? vr::Eye_Left : vr::Eye_Right;

				glm::vec2 playerViewClip = player.Get<ecs::View>()->clip;
				eyeEntity[i] = game->entityManager.NewEntity();
				auto eyeView = eyeEntity[i].Assign<ecs::View>();
				eyeView->extents = { vrWidth, vrHeight };
				eyeView->clip = playerViewClip;
				eyeView->vrEye = i + 1;
				auto projMatrix = vrSystem->GetProjectionMatrix(eyeType, eyeView->clip.x, eyeView->clip.y);
				eyeView->projMat = glm::transpose(glm::make_mat4((float *)projMatrix.m));
				auto eyePosOvr = vrSystem->GetEyeToHeadTransform(eyeType);
				eyePos[i] = glm::mat4(glm::make_mat3x4((float *)eyePosOvr.m));

				viewEntities.push_back(eyeEntity[i]);
			}
		}
		else if (vrSystem != nullptr)
		{
			vr::VR_Shutdown();
			vrSystem = nullptr;
		}
#endif

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
			auto player = scene->FindEntity("player");
			if (arg == "reset")
			{
				LoadScene(scene->name);
			}
			else if (player.Valid() && player.Has<ecs::Transform>())
			{
				// Store the player position and set it back on the new player entity
				auto transform = player.Get<ecs::Transform>();
				auto position = transform->GetPosition();
				auto rotation = transform->GetRotate();

				LoadScene(scene->name);

				if (scene)
				{
					player = scene->FindEntity("player");
					if (player.Valid() && player.Has<ecs::HumanController>())
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
		auto player = scene->FindEntity("player");
		if (player.Valid() && player.Has<ecs::Transform>() && player.Has<ecs::HumanController>())
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


	ecs::Entity GameLogic::GetPlayer()
	{
		return scene->FindEntity("player");
	}

	void GameLogic::OpenBarrier(string name)
	{
		if (!scene->namedEntities.count(name))
		{
			return Logf("%s not found", name);
		}

		auto ent = scene->namedEntities[name];
		if (!ent.Has<ecs::Barrier>())
		{
			return Logf("%s is not a barrier", name);
		}

		ecs::Barrier::Open(ent, game->physics);
	}

	void GameLogic::CloseBarrier(string name)
	{
		if (!scene->namedEntities.count(name))
		{
			return Logf("%s not found", name);
		}

		auto ent = scene->namedEntities[name];
		if (!ent.Has<ecs::Barrier>())
		{
			return Logf("%s is not a barrier", name);
		}

		ecs::Barrier::Close(ent, game->physics);
	}

	void GameLogic::OpenDoor(string name)
	{
		if (!scene->namedEntities.count(name))
		{
			return Logf("%s not found", name);
		}

		auto ent = scene->namedEntities[name];
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
		if (!scene->namedEntities.count(name))
		{
			return Logf("%s not found", name);
		}

		auto ent = scene->namedEntities[name];
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
