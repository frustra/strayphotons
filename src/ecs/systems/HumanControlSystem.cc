#define _USE_MATH_DEFINES
#include "ecs/systems/HumanControlSystem.hh"

#include "Common.hh"
#include "core/CVar.hh"
#include "core/Logging.hh"
#include "ecs/components/Interact.hh"
#include "ecs/components/Physics.hh"
#include "ecs/components/Transform.hh"
#include "physx/PhysxUtils.hh"

#include <PxRigidActor.h>
#include <PxScene.h>
#include <cmath>
#include <game/input/InputManager.hh>
#include <glm/glm.hpp>
#include <sstream>

namespace ecs {
	static sp::CVar<bool> CVarNoClip("p.NoClip", false, "Disable player clipping");
	static sp::CVar<float> CVarMovementSpeed("p.MovementSpeed", 3.0, "Player walking movement speed (m/s)");
	static sp::CVar<float> CVarSprintSpeed("p.SprintSpeed", 6.0, "Player sprinting movement speed (m/s)");
	static sp::CVar<float> CVarCrouchSpeed("p.CrouchSpeed", 1.5, "Player crouching movement speed (m/s)");
	static sp::CVar<float> CVarCursorSensitivity("p.CursorSensitivity", 1.0, "Mouse cursor sensitivity");

	HumanControlSystem::HumanControlSystem(
		ecs::EntityManager *entities, sp::InputManager *input, sp::PhysxManager *physics)
		: entities(entities), input(input), physics(physics) {}

	HumanControlSystem::~HumanControlSystem() {}

	bool HumanControlSystem::Frame(double dtSinceLastFrame) {
		if (input != nullptr && input->FocusLocked())
			return true;

		bool noclipChanged = CVarNoClip.Changed();
		auto noclip = CVarNoClip.Get(true);

		for (ecs::Entity entity : entities->EntitiesWith<ecs::Transform, ecs::HumanController>()) {
			// Handle keyboard controls
			glm::vec3 inputMovement = glm::vec3(0);
			bool jumping = false;
			bool sprinting = false;
			bool crouching = false;
			bool rotating = false;

			auto controller = entity.Get<ecs::HumanController>();

			if (input != nullptr) {
				if (input->IsDown(INPUT_ACTION_PLAYER_MOVE_FORWARD)) {
					inputMovement += glm::vec3(0, 0, -1);
				}

				if (input->IsDown(INPUT_ACTION_PLAYER_MOVE_BACKWARD)) {
					inputMovement += glm::vec3(0, 0, 1);
				}

				if (input->IsDown(INPUT_ACTION_PLAYER_MOVE_LEFT)) {
					inputMovement += glm::vec3(-1, 0, 0);
				}

				if (input->IsDown(INPUT_ACTION_PLAYER_MOVE_RIGHT)) {
					inputMovement += glm::vec3(1, 0, 0);
				}

				if (input->IsDown(INPUT_ACTION_PLAYER_MOVE_JUMP)) {
					if (noclip) {
						inputMovement += glm::vec3(0, 1, 0);
					} else {
						jumping = true;
					}
				}

				if (input->IsDown(INPUT_ACTION_PLAYER_MOVE_CROUCH)) {
					if (noclip) {
						inputMovement += glm::vec3(0, -1, 0);
					} else {
						crouching = true;
					}
				}

				if (input->IsDown(INPUT_ACTION_PLAYER_MOVE_SPRINT)) {
					sprinting = true;
				}

				if (input->IsPressed(INPUT_ACTION_PLAYER_INTERACT)) {
					Interact(entity, dtSinceLastFrame);
				}

				if (input->IsDown(INPUT_ACTION_PLAYER_INTERACT_ROTATE)) {
					rotating = true;
				}

				// Handle mouse controls
				const glm::vec2 *cursorPos, *cursorPosPrev;
				if (input->GetActionDelta(sp::INPUT_ACTION_MOUSE_CURSOR, &cursorPos, &cursorPosPrev)) {
					glm::vec2 cursorDiff = *cursorPos;
					if (cursorPosPrev != nullptr) {
						cursorDiff -= *cursorPosPrev;
					}
					if (!rotating || !InteractRotate(entity, dtSinceLastFrame, cursorDiff)) {
						float sensitivity = CVarCursorSensitivity.Get() * 0.001;
						controller->yaw -= cursorDiff.x * sensitivity;
						if (controller->yaw > 2.0f * M_PI) {
							controller->yaw -= 2.0f * M_PI;
						}
						if (controller->yaw < 0) {
							controller->yaw += 2.0f * M_PI;
						}

						controller->pitch -= cursorDiff.y * sensitivity;

						const float feps = std::numeric_limits<float>::epsilon();
						controller->pitch =
							std::max(-((float)M_PI_2 - feps), std::min(controller->pitch, (float)M_PI_2 - feps));

						auto transform = entity.Get<ecs::Transform>();
						transform->SetRotate(
							glm::quat(glm::vec3(controller->pitch, controller->yaw, controller->roll)));
					}
				}
			}

			// Move the player
			if (noclipChanged) {
				physics->ToggleCollisions(controller->pxController->getActor(), !noclip);

				physx::PxShape *shape;
				controller->pxController->getActor()->getShapes(&shape, 1);
				physx::PxFilterData data;
				data.word0 = noclip ? sp::PhysxCollisionGroup::NOCLIP : sp::PhysxCollisionGroup::PLAYER;
				shape->setQueryFilterData(data);
				shape->setSimulationFilterData(data);
			}

			auto currentHeight = physics->GetCapsuleHeight(controller->pxController);
			auto targetHeight = crouching ? ecs::PLAYER_CAPSULE_CROUCH_HEIGHT : ecs::PLAYER_CAPSULE_HEIGHT;
			if (fabs(targetHeight - currentHeight) > 0.1) {
				// If player is in the air, resize from the top to implement crouch-jumping.
				auto newHeight = currentHeight + (targetHeight - currentHeight) * (controller->onGround ? 0.1 : 1.0);
				ResizeEntity(entity, newHeight, !controller->onGround);
			}

			auto velocity =
				CalculatePlayerVelocity(entity, dtSinceLastFrame, inputMovement, jumping, sprinting, crouching);
			MoveEntity(entity, dtSinceLastFrame, velocity);
		}

		return true;
	}

	ecs::Handle<HumanController> HumanControlSystem::AssignController(ecs::Entity entity, sp::PhysxManager &px) {
		if (entity.Has<HumanController>()) {
			std::stringstream ss;
			ss << "entity " << entity << " cannot be assigned a new HumanController because it already has one.";
			throw std::invalid_argument(ss.str());
		}
		auto transform = entity.Get<ecs::Transform>();
		glm::quat rotation = transform->GetRotate();

		auto controller = entity.Assign<HumanController>();
		controller->SetRotate(rotation);

		auto interact = entity.Assign<InteractController>();
		interact->manager = &px;

		// Offset the capsule position so the camera is at the top
		physx::PxVec3 pos = GlmVec3ToPxVec3(transform->GetPosition() - glm::vec3(0, ecs::PLAYER_CAPSULE_HEIGHT / 2, 0));
		controller->pxController = px.CreateController(pos, ecs::PLAYER_RADIUS, ecs::PLAYER_CAPSULE_HEIGHT, 0.5f);
		controller->pxController->setStepOffset(ecs::PLAYER_STEP_HEIGHT);

		return controller;
	}

	void HumanControlSystem::Teleport(ecs::Entity entity, glm::vec3 position, glm::quat rotation) {
		if (!entity.Has<ecs::Transform>()) {
			throw std::invalid_argument("entity must have a Transform component");
		}
		if (!entity.Has<ecs::HumanController>()) {
			throw std::invalid_argument("entity must have a HumanController component");
		}

		auto controller = entity.Get<ecs::HumanController>();
		auto transform = entity.Get<ecs::Transform>();
		transform->SetPosition(position);
		if (rotation != glm::quat()) {
			transform->SetRotate(rotation);
			controller->SetRotate(rotation);
		}

		if (controller->pxController) {
			// Offset the capsule position so the camera is at the top
			float capsuleHeight = physics->GetCapsuleHeight(controller->pxController);
			physics->TeleportController(controller->pxController,
				GlmVec3ToPxExtendedVec3(position - glm::vec3(0, capsuleHeight / 2, 0)));
		}
	}

	glm::vec3 HumanControlSystem::CalculatePlayerVelocity(
		ecs::Entity entity, double dtSinceLastFrame, glm::vec3 inDirection, bool jump, bool sprint, bool crouch) {
		if (!entity.Has<ecs::Transform>()) {
			throw std::invalid_argument("entity must have a Transform component");
		}

		auto noclip = CVarNoClip.Get();
		auto controller = entity.Get<ecs::HumanController>();
		auto transform = entity.Get<ecs::Transform>();

		glm::vec3 movement = transform->GetRotate() * glm::vec3(inDirection.x, 0, inDirection.z);

		if (!noclip) {
			if (std::abs(movement.y) > 0.999) {
				movement = transform->GetRotate() * glm::vec3(0, -movement.y, 0);
			}
			movement.y = 0;
		}
		if (movement != glm::vec3(0)) {
			float speed = CVarMovementSpeed.Get();
			if (sprint && controller->onGround)
				speed = CVarSprintSpeed.Get();
			if (crouch && controller->onGround)
				speed = CVarCrouchSpeed.Get();
			movement = glm::normalize(movement) * speed;
		}
		movement.y += inDirection.y * CVarMovementSpeed.Get();

		if (noclip) {
			controller->velocity = movement;
			return controller->velocity;
		}
		if (controller->onGround) {
			controller->velocity.x = movement.x;
			controller->velocity.y -= 0.01; // Always try moving down so that onGround detection is more consistent.
			if (jump)
				controller->velocity.y = ecs::PLAYER_JUMP_VELOCITY;
			controller->velocity.z = movement.z;
		} else {
			controller->velocity += movement * ecs::PLAYER_AIR_STRAFE * (float)dtSinceLastFrame;
			controller->velocity.y -= ecs::PLAYER_GRAVITY * dtSinceLastFrame;
		}

		return controller->velocity;
	}

	void HumanControlSystem::MoveEntity(ecs::Entity entity, double dtSinceLastFrame, glm::vec3 velocity) {
		auto transform = entity.Get<ecs::Transform>();
		auto controller = entity.Get<HumanController>();

		if (controller->pxController) {
			auto disp = velocity * (float)dtSinceLastFrame;
			auto prevPosition = PxExtendedVec3ToGlmVec3P(controller->pxController->getPosition());
			if (CVarNoClip.Get()) {
				physics->TeleportController(controller->pxController, GlmVec3ToPxExtendedVec3(prevPosition + disp));
				controller->onGround = true;
			} else {
				controller->onGround =
					physics->MoveController(controller->pxController, dtSinceLastFrame, GlmVec3ToPxVec3(disp));
			}
			auto newPosition = PxExtendedVec3ToGlmVec3P(controller->pxController->getPosition());
			// Don't accelerate more than our current velocity
			auto velocityPosition = glm::min(prevPosition + glm::abs(disp), newPosition);
			velocityPosition = glm::max(prevPosition - glm::abs(disp), velocityPosition);

			// Update the velocity based on what happened in physx
			controller->velocity = (velocityPosition - prevPosition) / (float)dtSinceLastFrame;
			glm::vec3 *velocity = (glm::vec3 *)controller->pxController->getUserData();
			*velocity = CVarNoClip.Get() ? glm::vec3(0) : controller->velocity;

			// Offset the capsule position so the camera is at the top
			float capsuleHeight = physics->GetCapsuleHeight(controller->pxController);
			transform->SetPosition(newPosition + glm::vec3(0, capsuleHeight / 2, 0));
		}
	}

	bool HumanControlSystem::ResizeEntity(ecs::Entity entity, float height, bool fromTop) {
		auto transform = entity.Get<ecs::Transform>();
		auto controller = entity.Get<HumanController>();

		physx::PxCapsuleController *pxController = controller->pxController;
		if (pxController) {
			float oldHeight = physics->GetCapsuleHeight(pxController);
			physics->ResizeController(pxController, height, fromTop);

			physx::PxOverlapBuffer hit;
			physx::PxRigidDynamic *actor = pxController->getActor();

			bool valid = !physics->OverlapQuery(actor, physx::PxVec3(0), hit);

			if (!valid) {
				physics->ResizeController(pxController, oldHeight, fromTop);
			}
			return valid;
		}
		return false;
	}

	void HumanControlSystem::Interact(ecs::Entity entity, double dt) {
		auto interact = entity.Get<ecs::InteractController>();
		interact->PickUpObject(entity);
	}

	bool HumanControlSystem::InteractRotate(ecs::Entity entity, double dt, glm::vec2 dCursor) {
		auto interact = entity.Get<ecs::InteractController>();
		auto transform = entity.Get<ecs::Transform>();
		if (interact->target) {
			auto rotation = glm::vec3(dCursor.y, dCursor.x, 0) * (float)(CVarCursorSensitivity.Get() * 0.1 * dt);
			physics->RotateConstraint(entity, interact->target, GlmVec3ToPxVec3(rotation));
			return true;
		}
		return false;
	}
} // namespace ecs
