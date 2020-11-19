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
#include <ecs/Components.hh>
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
		funcs.Register(this,
			"setvrorigin",
			"Move the VR origin to the current player position",
			&GameLogic::SetVrOrigin);

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

	static CVar<bool> CVarConnectXR("xr.Connect", true, "Connect to a supported XR Runtime");
	static CVar<bool> CVarController("xr.Controllers", true, "Render controller models (if available)");

	enum SkeletonMode { NONE = 0, NORMAL = 1, DEBUG = 2 };
	static CVar<int> CVarSkeletons("xr.Skeletons", 1, "XR Skeleton mode (0: none, 1: normal, 2: debug)");

	void GameLogic::InitXrActions() {
		gameActionSet = xrSystem->GetActionSet(xr::GameActionSet);

		// Create teleport action
		teleportAction = gameActionSet->CreateAction(xr::TeleportActionName, xr::XrActionType::Bool);

		// Suggested bindings for Oculus Touch
		teleportAction->AddSuggestedBinding("/interaction_profiles/oculus/touch_controller",
			"/user/hand/right/input/a/click");

		// Suggested bindings for Valve Index
		teleportAction->AddSuggestedBinding("/interaction_profiles/valve/index_controller",
			"/user/hand/right/input/trigger/click");

		// Suggested bindings for HTC Vive
		teleportAction->AddSuggestedBinding("/interaction_profiles/htc/vive_controller",
			"/user/hand/right/input/trackpad/click");

		// Create grab / interract action
		grabAction = gameActionSet->CreateAction(xr::GrabActionName, xr::XrActionType::Bool);

		// Suggested bindings for Oculus Touch
		grabAction->AddSuggestedBinding("/interaction_profiles/oculus/touch_controller",
			"/user/hand/left/input/squeeze/value");
		grabAction->AddSuggestedBinding("/interaction_profiles/oculus/touch_controller",
			"/user/hand/right/input/squeeze/value");

		// Suggested bindings for Valve Index
		grabAction->AddSuggestedBinding("/interaction_profiles/valve/index_controller",
			"/user/hand/left/input/squeeze/click");
		grabAction->AddSuggestedBinding("/interaction_profiles/valve/index_controller",
			"/user/hand/right/input/squeeze/click");

		// Suggested bindings for HTC Vive
		grabAction->AddSuggestedBinding("/interaction_profiles/htc/vive_controller",
			"/user/hand/left/input/squeeze/click");
		grabAction->AddSuggestedBinding("/interaction_profiles/htc/vive_controller",
			"/user/hand/right/input/squeeze/click");

		// Create LeftHand Pose action
		leftHandAction = gameActionSet->CreateAction(xr::LeftHandActionName, xr::XrActionType::Pose);
		leftHandAction->AddSuggestedBinding("/interaction_profiles/oculus/touch_controller",
			"/user/hand/left/input/grip/pose");
		leftHandAction->AddSuggestedBinding("/interaction_profiles/valve/index_controller",
			"/user/hand/left/input/grip/pose");
		leftHandAction->AddSuggestedBinding("/interaction_profiles/htc/vive_controller",
			"/user/hand/left/input/grip/pose");

		// Create RightHand Pose action
		rightHandAction = gameActionSet->CreateAction(xr::RightHandActionName, xr::XrActionType::Pose);
		rightHandAction->AddSuggestedBinding("/interaction_profiles/oculus/touch_controller",
			"/user/hand/right/input/grip/pose");
		rightHandAction->AddSuggestedBinding("/interaction_profiles/valve/index_controller",
			"/user/hand/right/input/grip/pose");
		rightHandAction->AddSuggestedBinding("/interaction_profiles/htc/vive_controller",
			"/user/hand/right/input/grip/pose");

		// Create LeftHand Skeleton action
		leftHandSkeletonAction =
			gameActionSet->CreateAction(xr::LeftHandSkeletonActionName, xr::XrActionType::Skeleton);

		// TODO: add suggested bindings for real XR backends when OpenXR supports skeletons

		// Create RightHand Skeleton action
		rightHandSkeletonAction =
			gameActionSet->CreateAction(xr::RightHandSkeletonActionName, xr::XrActionType::Skeleton);

		// TODO: add suggested bindings for real XR backends when OpenXR supports skeletons
	}

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
			auto entity = game->entityManager.NewEntity();
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

		// TODO: move this into XrSystem as part of #39
		// Handle xr controller movement
		if (xrSystem) {
			ecs::Entity vrOrigin = game->entityManager.EntityWith<ecs::Name>("vr-origin");

			if (vrOrigin.Valid()) {
				auto vrOriginTransform = vrOrigin.Get<ecs::Transform>();

				// TODO: support other action sets
				gameActionSet->Sync();

				// Mapping of Pose Actions to Subpaths. Needed so we can tell which-hand-did-what for the hand pose
				// linked actions
				vector<std::pair<xr::XrActionPtr, string>> controllerPoseActions = {
					{leftHandAction, xr::SubpathLeftHand},
					{rightHandAction, xr::SubpathRightHand}};

				for (auto controllerAction : controllerPoseActions) {
					glm::mat4 xrObjectPos;
					bool active =
						controllerAction.first->GetPoseActionValueForNextFrame(controllerAction.second, xrObjectPos);
					ecs::Entity xrObject = UpdateXrActionEntity(controllerAction.first, active && CVarController.Get());

					if (xrObject.Valid()) {
						xrObjectPos = glm::transpose(
							xrObjectPos * glm::transpose(vrOriginTransform->GetGlobalTransform(game->entityManager)));

						auto ctrl = xrObject.Get<ecs::Transform>();
						ctrl->SetPosition(xrObjectPos * glm::vec4(0, 0, 0, 1));
						ctrl->SetRotate(glm::mat4(glm::mat3(xrObjectPos)));

						if (controllerAction.first == rightHandAction) {
							// TODO: make this bound to the "dominant user hand", or pick the last hand the user tried
							// to teleport with
							// TODO: make this support "skeleton-only" mode
							// FIXME: the laser pointer is now affected by shadows and is no longer emissive. #40
							// Parent the laser pointer to the xrObject representing the last teleport action
							ecs::Entity laserPointer = GetLaserPointer();

							if (laserPointer.Valid()) {
								auto transform = laserPointer.Get<ecs::Transform>();
								auto parent = xrObject;

								transform->SetPosition(glm::vec3(0, 0, 0));
								transform->SetRotate(glm::quat());
								transform->SetParent(parent);
							}
						}

						bool teleport = false;
						teleportAction->GetRisingEdgeActionValue(controllerAction.second, teleport);

						if (teleport) {
							Logf("Teleport on subpath %s", controllerAction.second);

							auto origin = GlmVec3ToPxVec3(ctrl->GetPosition());
							auto dir = GlmVec3ToPxVec3(ctrl->GetForward());
							dir.normalizeSafe();
							physx::PxReal maxDistance = 10.0f;

							physx::PxRaycastBuffer hit;
							bool status = game->physics.RaycastQuery(xrObject, origin, dir, maxDistance, hit);

							if (status && hit.block.distance > 0.5) {
								auto headPos =
									glm::vec3(xrObjectPos * glm::vec4(0, 0, 0, 1)) - vrOriginTransform->GetPosition();
								auto newPos =
									PxVec3ToGlmVec3P(origin + dir * std::max(0.0, hit.block.distance - 0.5)) - headPos;
								vrOriginTransform->SetPosition(
									glm::vec3(newPos.x, vrOriginTransform->GetPosition().y, newPos.z));
							}
						}

						auto interact = xrObject.Get<ecs::InteractController>();
						bool grab = false;
						bool let_go = false;

						grabAction->GetRisingEdgeActionValue(controllerAction.second, grab);
						grabAction->GetFallingEdgeActionValue(controllerAction.second, let_go);

						if (grab) {
							Logf("grab on subpath %s", controllerAction.second);
							interact->PickUpObject(xrObject);
						} else if (let_go) {
							Logf("Let go on subpath %s", controllerAction.second);
							if (interact->target) {
								interact->manager->RemoveConstraint(xrObject, interact->target);
								interact->target = nullptr;
							}
						}
					}
				}

				// Now move generic tracked objects around (HMD, Vive Pucks, etc..)
				for (xr::TrackedObjectHandle trackedObjectHandle : xrSystem->GetTracking()->GetTrackedObjectHandles()) {
					ecs::Entity xrObject = ValidateAndLoadTrackedObject(trackedObjectHandle);

					if (xrObject.Valid()) {
						glm::mat4 xrObjectPos;

						if (xrSystem->GetTracking()->GetPredictedObjectPose(trackedObjectHandle, xrObjectPos)) {
							xrObjectPos = glm::transpose(
								xrObjectPos *
								glm::transpose(vrOriginTransform->GetGlobalTransform(game->entityManager)));

							auto ctrl = xrObject.Get<ecs::Transform>();
							ctrl->SetPosition(xrObjectPos * glm::vec4(0, 0, 0, 1));
							ctrl->SetRotate(glm::mat4(glm::mat3(xrObjectPos)));
						}
					}
				}

				if (CVarSkeletons.Get() != SkeletonMode::NONE) {
					// Now load skeletons
					for (xr::XrActionPtr action : {leftHandSkeletonAction, rightHandSkeletonAction}) {
						// Get the action pose
						glm::mat4 xrObjectPos;
						bool activePose = action->GetPoseActionValueForNextFrame(xr::SubpathNone, xrObjectPos);

						if (!activePose) {
							// Can't do anything without a hand pose. Bail out early, destroying all skeleton
							// entities on the way out
							UpdateXrActionEntity(action, false);
							continue;
						}

						xrObjectPos = glm::transpose(
							xrObjectPos * glm::transpose(vrOriginTransform->GetGlobalTransform(game->entityManager)));

						// Get the bone data (optionally with or without an attached controller)
						vector<xr::XrBoneData> boneData;
						bool activeSkeleton = action->GetSkeletonActionValue(boneData, CVarController.Get());

						if (!activeSkeleton) {
							// Can't do anything without bone data. Bail out early, destroying all skeleton
							// entities on the way out
							UpdateXrActionEntity(action, false);
							continue;
						}

						// Update the state of the "normal" skeleton entity (the RenderModel provided by the XR Runtime)
						ecs::Entity handSkeleton =
							UpdateXrActionEntity(action, CVarSkeletons.Get() == SkeletonMode::NORMAL);
						if (handSkeleton.Valid()) {
							auto hand = handSkeleton.Get<ecs::Renderable>();

							ComputeBonePositions(boneData, hand->model->bones);

							auto ctrl = handSkeleton.Get<ecs::Transform>();

							// Extract just the position from the mat4
							ctrl->SetPosition(xrObjectPos * glm::vec4(0, 0, 0, 1));
							ctrl->SetRotate(glm::mat4(glm::mat3(xrObjectPos)));
						}

						// TODO: this checks the state of ~30 entities by name.
						// Optimize this into separate functions for setup, teardown, and position updates. #39
						UpdateSkeletonDebugHand(action,
							xrObjectPos,
							boneData,
							CVarSkeletons.Get() == SkeletonMode::DEBUG);
					}
				}
			}
		}

		if (!humanControlSystem.Frame(dtSinceLastFrame)) return false;
		if (!lightGunSystem.Frame(dtSinceLastFrame)) return false;
		if (!doorSystem.Frame(dtSinceLastFrame)) return false;

		return true;
	}

	// TODO: move this into XrSystem as part of #39
	void GameLogic::ComputeBonePositions(vector<xr::XrBoneData> &boneData, vector<glm::mat4> &output) {
		// Resize the output vector to match the number of joints the GLTF will reference
		output.resize(boneData.size());

		for (int i = 0; i < boneData.size(); i++) {
			xr::XrBoneData &bone = boneData[i];

			glm::mat4 rot_mat = glm::mat4_cast(bone.rot);
			glm::mat4 trans_mat = glm::translate(bone.pos);
			glm::mat4 pose = trans_mat * rot_mat;
			output[i] = pose * bone.inverseBindPose;
		}
	}

	// TODO: move this into XrSystem as part of #39
	ecs::Entity GameLogic::ValidateAndLoadTrackedObject(sp::xr::TrackedObjectHandle &trackedObjectHandle) {
		string entityName = trackedObjectHandle.name;
		ecs::Entity xrObject = game->entityManager.EntityWith<ecs::Name>(entityName);

		if (trackedObjectHandle.connected) {
			// Make sure object is valid
			if (!xrObject.Valid()) {
				xrObject = game->entityManager.NewEntity();
				xrObject.Assign<ecs::Name>(entityName);
			}

			if (!xrObject.Has<ecs::Transform>()) { xrObject.Assign<ecs::Transform>(); }

			if (!xrObject.Has<ecs::Renderable>()) {
				auto renderable = xrObject.Assign<ecs::Renderable>();
				renderable->model = xrSystem->GetTracking()->GetTrackedObjectModel(trackedObjectHandle);

				// TODO: better handling for failed model loads
				Assert((bool)(renderable->model), "Failed to load skeleton model");

				// Rendering an XR HMD model from the viewpoint of an XRView is a bad idea
				if (trackedObjectHandle.type == xr::HMD) { renderable->xrExcluded = true; }
			}

			// Mark the XR HMD as being able to activate TriggerAreas
			if (trackedObjectHandle.type == xr::HMD && !xrObject.Has<ecs::Triggerable>()) {
				xrObject.Assign<ecs::Triggerable>();
			}
		} else {
			if (xrObject.Valid()) { xrObject.Destroy(); }
		}

		return xrObject;
	}

	// Validate and load the skeleton debug hand entities for a skeleton action. If the action is not "active", this
	// destroys the debug hand entities.
	void GameLogic::UpdateSkeletonDebugHand(
		xr::XrActionPtr action, glm::mat4 xrObjectPos, vector<xr::XrBoneData> &boneData, bool active) {
		for (int i = 0; i < boneData.size(); i++) {
			xr::XrBoneData &bone = boneData[i];
			string entityName = string("xr-skeleton-debug-bone-") + action->GetName() + std::to_string(i);

			ecs::Entity boneEntity = game->entityManager.EntityWith<ecs::Name>(entityName);

			if (active) {
				if (!boneEntity.Valid()) {
					boneEntity = game->entityManager.NewEntity();
					boneEntity.Assign<ecs::Name>(entityName);
				}

				if (!boneEntity.Has<ecs::Transform>()) { boneEntity.Assign<ecs::Transform>(); }

				if (!boneEntity.Has<ecs::InteractController>()) {
					auto interact = boneEntity.Assign<ecs::InteractController>();
					interact->manager = &game->physics;
				}

				if (!boneEntity.Has<ecs::Renderable>()) {
					auto model = GAssets.LoadModel("box");
					auto renderable = boneEntity.Assign<ecs::Renderable>(model);

					// TODO: better handling for failed model loads
					Assert((bool)(renderable->model), "Failed to load skeleton model");
				}

				auto ctrl = boneEntity.Get<ecs::Transform>();
				ctrl->SetScale(glm::vec3(0.01f));
				ctrl->SetPosition(xrObjectPos * glm::vec4(bone.pos.x, bone.pos.y, bone.pos.z, 1));
				ctrl->SetRotate(glm::toMat4(bone.rot) * glm::mat4(glm::mat3(xrObjectPos)));
			} else {
				if (boneEntity.Valid()) { boneEntity.Destroy(); }
			}
		}
	}

	// Validate and load the entity and model associated with an action. If the action is not "active", this destroys
	// the entity and model.
	ecs::Entity GameLogic::UpdateXrActionEntity(xr::XrActionPtr action, bool active) {
		string entityName = "xr-action-" + action->GetName();
		ecs::Entity xrObject = game->entityManager.EntityWith<ecs::Name>(entityName);

		// Test that the xrObject correctly reflects the "active" state
		if (active) {
			// Make sure object is valid
			if (!xrObject.Valid()) {
				xrObject = game->entityManager.NewEntity();
				xrObject.Assign<ecs::Name>(entityName);
			}

			if (!xrObject.Has<ecs::Transform>()) { xrObject.Assign<ecs::Transform>(); }

			if (!xrObject.Has<ecs::InteractController>()) {
				auto interact = xrObject.Assign<ecs::InteractController>();
				interact->manager = &game->physics;
			}

			// XrAction models might take many frames to load.
			// We constantly re-check the state of this entity while it's active
			// to continue trying to load the model from the underlying XR Runtime,
			// since it's likely being loaded from disk asyncrhonously and will eventually
			// become available.
			if (!xrObject.Has<ecs::Renderable>()) {
				shared_ptr<Model> inputSourceModel = action->GetInputSourceModel();

				if (inputSourceModel) {
					auto renderable = xrObject.Assign<ecs::Renderable>();
					renderable->model = inputSourceModel;
				}
			}
		}
		// Completely destroy inactive input sources so that resources are freed (if possible)
		else {
			if (xrObject.Valid()) { xrObject.Destroy(); }
		}

		return xrObject;
	}

	ecs::Entity GameLogic::GetLaserPointer() {
		string entityName = "xr-laser-pointer";
		ecs::Entity xrObject = game->entityManager.EntityWith<ecs::Name>(entityName);

		// Make sure object is valid
		if (!xrObject.Valid()) {
			xrObject = game->entityManager.NewEntity();
			xrObject.Assign<ecs::Name>(entityName);
		}

		if (!xrObject.Has<ecs::Transform>()) { xrObject.Assign<ecs::Transform>(); }

		if (!xrObject.Has<ecs::Renderable>()) {
			auto renderable = xrObject.Assign<ecs::Renderable>();
			shared_ptr<BasicModel> model = make_shared<BasicModel>("laser-pointer-beam");
			renderable->model = model;

			// 10 unit long line
			glm::vec3 start = glm::vec3(0, 0, 0);
			glm::vec3 end = glm::vec3(0, 0, -10.0);
			float lineWidth = 0.001f;

			// Doesn't have to persist
			vector<SceneVertex> vertices;

			glm::vec3 lineDir = glm::normalize(end - start);
			glm::vec3 widthVec = lineWidth * glm::vec3(1.0, 0.0, 0.0);

			// move the positions back a bit to account for overlapping lines
			glm::vec3 pos0 = start; // start - lineWidth * lineDir;
			glm::vec3 pos1 = end + lineWidth * lineDir;

			auto addVertex = [&](const glm::vec3 &pos) {
				vertices.push_back({{pos.x, pos.y, pos.z}, glm::vec3(0.0, 1.0, 0.0), {0, 0}});
			};

			// 2 triangles that make up a "fat" line connecting pos0 and pos1
			// with the flat face pointing at the player
			addVertex(pos0 - widthVec);
			addVertex(pos1 + widthVec);
			addVertex(pos0 + widthVec);

			addVertex(pos1 - widthVec);
			addVertex(pos1 + widthVec);
			addVertex(pos0 - widthVec);

			// Create the data for the GPU
			unsigned char baseColor[4] = {255, 0, 0, 255};

			model->basicMaterials["red_laser"] = BasicMaterial(baseColor);
			BasicMaterial &mat = model->basicMaterials["red_laser"];

			GLModel::Primitive prim;

			model->vbos.try_emplace("beam");
			VertexBuffer &vbo = model->vbos["beam"];

			model->ibos.try_emplace("beam");
			Buffer &ibo = model->ibos["beam"];

			const vector<uint16_t> indexData = {0, 1, 2, 3, 4, 5, 2, 1, 0, 5, 4, 3};

			// Model class will delete this on destruction
			Model::Primitive *sourcePrim = new Model::Primitive;

			prim.parent = sourcePrim;
			prim.baseColorTex = &mat.baseColorTex;
			prim.metallicRoughnessTex = &mat.metallicRoughnessTex;
			prim.heightTex = &mat.heightTex;

			vbo.SetElementsVAO(vertices.size(), vertices.data(), GL_DYNAMIC_DRAW);
			prim.vertexBufferHandle = vbo.VAO();

			ibo.Create().Data(indexData.size() * sizeof(uint16_t), indexData.data());
			prim.indexBufferHandle = ibo.handle;

			sourcePrim->drawMode = GL_TRIANGLES;
			sourcePrim->indexBuffer.byteOffset = 0;
			sourcePrim->indexBuffer.components = indexData.size();
			sourcePrim->indexBuffer.componentType = GL_UNSIGNED_SHORT;

			renderable->model->glModel = make_shared<GLModel>(renderable->model.get(), game->graphics.GetContext());
			renderable->model->glModel->AddPrimitive(prim);
		}

		return xrObject;
	}

	void GameLogic::LoadScene(string name) {
		game->graphics.RenderLoading();
		game->physics.StopSimulation();
		game->entityManager.DestroyAll();

		if (scene != nullptr) {
			for (auto &line : scene->unloadExecList) {
				GetConsoleManager().ParseAndExecute(line);
			}
		}

		scene.reset();
		scene = GAssets.LoadScene(name, &game->entityManager, game->physics);
		if (!scene) {
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

		// On scene load, check if the status of xrSystem matches the status of CVarConnectXR
		if (CVarConnectXR.Get()) {
			// No xrSystem loaded but CVarConnectXR indicates we should try to load one.
			if (!xrSystem) {
				// TODO: refactor this so that xrSystemFactory.GetBestXrSystem() returns a TypeTrait
				xr::XrSystemFactory xrSystemFactory;
				xrSystem = xrSystemFactory.GetBestXrSystem();

				if (xrSystem) {
					try {
						xrSystem->Init();
					} catch (std::exception e) {
						Errorf("XR Runtime threw error on initialization! Error: %s", e.what());
						xrSystem.reset();
					}
				} else {
					Logf("Failed to load an XR runtime");
				}
			}

			// The previous step might have failed to load an XR system, test it is loaded
			if (xrSystem) {
				// Create a VR origin if one does not already exist
				ecs::Entity vrOrigin = game->entityManager.EntityWith<ecs::Name>("vr-origin");
				if (!vrOrigin.Valid()) {
					vrOrigin = game->entityManager.NewEntity();
					// Use AssignKey so we can find this entity by name later
					vrOrigin.Assign<ecs::Name>("vr-origin");
				}

				// Add a transform to the VR origin if one does not already exist
				if (!vrOrigin.Has<ecs::Transform>()) {
					auto transform = vrOrigin.Assign<ecs::Transform>();
					if (player.Valid() && player.Has<ecs::Transform>()) {
						auto playerTransform = player.Get<ecs::Transform>();
						transform->SetPosition(playerTransform->GetGlobalPosition(game->entityManager) -
											   glm::vec3(0, ecs::PLAYER_CAPSULE_HEIGHT, 0));
						transform->SetRotate(playerTransform->GetRotate());
					}
				}

				// Create swapchains for all the views this runtime exposes. Only create the minimum number of views,
				// since it's unlikely we can render more than the bare minimum. Ex: we don't support 3rd eye rendering
				// for mixed reality capture
				// TODO: add a CVar to allow 3rd eye rendering
				for (unsigned int i = 0; i < xrSystem->GetCompositor()->GetNumViews(true /* minimum */); i++) {
					ecs::Entity viewEntity = game->entityManager.NewEntity();
					auto ecsView = viewEntity.Assign<ecs::View>();
					xrSystem->GetCompositor()->PopulateView(i, ecsView);

					// Mark this as an XR View
					auto ecsXrView = viewEntity.Assign<ecs::XRView>();
					ecsXrView->viewId = i;

					viewEntities.push_back(viewEntity);
				}

				// TODO: test this re-initializes correctly
				InitXrActions();
			}
		}
		// CVar says XR should be disabled. Ensure xrSystem state matches.
		else if (xrSystem) {
			xrSystem->Deinit();
			xrSystem.reset();
		}

		game->graphics.SetPlayerView(viewEntities);

		for (auto &line : scene->autoExecList) {
			GetConsoleManager().ParseAndExecute(line);
		}

		// Create flashlight entity
		flashlight = game->entityManager.NewEntity();
		{
			auto transform = flashlight.Assign<ecs::Transform>(glm::vec3(0, -0.3, 0));
			transform->SetParent(player);
		}
		{
			auto light = flashlight.Assign<ecs::Light>();
			light->tint = glm::vec3(1.0);
			light->spotAngle = glm::radians(CVarFlashlightAngle.Get(true));
			light->intensity = CVarFlashlight.Get(true);
			light->on = CVarFlashlightOn.Get(true);
		}
		{
			auto view = flashlight.Assign<ecs::View>();
			view->fov = glm::radians(CVarFlashlightAngle.Get(true)) * 2.0f;
			view->extents = glm::vec2(CVarFlashlightResolution.Get(true));
			view->clip = glm::vec2(0.1, 64);
		}

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

	void GameLogic::SetVrOrigin() {
		if (CVarConnectXR.Get()) {
			Logf("Resetting VR Origin");
			auto vrOrigin = game->entityManager.EntityWith<ecs::Name>("vr-origin");
			auto player = GetPlayer();
			if (vrOrigin && vrOrigin.Has<ecs::Transform>() && player && player.Has<ecs::Transform>()) {
				auto vrTransform = vrOrigin.Get<ecs::Transform>();
				auto playerTransform = player.Get<ecs::Transform>();

				vrTransform->SetPosition(playerTransform->GetGlobalPosition(game->entityManager) -
										 glm::vec3(0, ecs::PLAYER_CAPSULE_HEIGHT, 0));
			}
		}
	}

	shared_ptr<xr::XrSystem> GameLogic::GetXrSystem() {
		return xrSystem;
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
} // namespace sp
