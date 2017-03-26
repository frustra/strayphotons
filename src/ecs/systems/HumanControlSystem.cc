#define _USE_MATH_DEFINES
#include <cmath>

#include "ecs/systems/HumanControlSystem.hh"
#include "ecs/components/Interact.hh"
#include "ecs/components/Transform.hh"
#include "ecs/components/Physics.hh"

#include "physx/PhysxUtils.hh"

#include "core/Logging.hh"
#include "Common.hh"
#include "core/CVar.hh"
#include "core/Logging.hh"

#include <glm/glm.hpp>
#include <GLFW/glfw3.h>
#include <PxRigidActor.h>
#include <PxScene.h>

#include <sstream>

namespace ecs
{
	static sp::CVar<bool> CVarNoClip("p.NoClip", false, "Disable player clipping");
	static sp::CVar<float> CVarMovementSpeed("p.MovementSpeed", 3.0, "Player walking movement speed (m/s)");
	static sp::CVar<float> CVarSprintSpeed("p.SprintSpeed", 6.0, "Player sprinting movement speed (m/s)");
	static sp::CVar<float> CVarCrouchSpeed("p.CrouchSpeed", 1.5, "Player crouching movement speed (m/s)");
	static sp::CVar<float> CVarCursorSensitivity("p.CursorSensitivity", 1.0, "Mouse cursor sensitivity");

	HumanControlSystem::HumanControlSystem(ecs::EntityManager *entities, sp::InputManager *input, sp::PhysxManager *physics)
		: entities(entities), input(input), physics(physics)
	{
	}

	HumanControlSystem::~HumanControlSystem()
	{
	}

	bool HumanControlSystem::Frame(double dtSinceLastFrame)
	{
		if (input->FocusLocked())
			return true;

		for (ecs::Entity entity : entities->EntitiesWith<ecs::Transform, ecs::HumanController>())
		{
			// Handle keyboard controls
			auto noclip = CVarNoClip.Get();
			glm::vec3 inputMovement = glm::vec3(0);
			bool jumping = false;
			bool sprinting = false;
			bool crouching = false;
			bool rotating = false;

			for (auto const &actionKeysPair : entity.Get<ecs::HumanController>()->inputMap)
			{
				ControlAction action = actionKeysPair.first;
				const vector<int> &keys = actionKeysPair.second;

				if (!input->IsAnyDown(keys))
				{
					continue;
				}

				switch (action)
				{
					case ControlAction::MOVE_FORWARD:
						inputMovement += glm::vec3(0, 0, -1);
						break;
					case ControlAction::MOVE_BACKWARD:
						inputMovement += glm::vec3(0, 0, 1);
						break;
					case ControlAction::MOVE_LEFT:
						inputMovement += glm::vec3(-1, 0, 0);
						break;
					case ControlAction::MOVE_RIGHT:
						inputMovement += glm::vec3(1, 0, 0);
						break;
					case ControlAction::MOVE_JUMP:
						if (noclip)
						{
							inputMovement += glm::vec3(0, 1, 0);
						}
						else
						{
							jumping = true;
						}
						break;
					case ControlAction::MOVE_SPRINT:
						sprinting = true;
						break;
					case ControlAction::INTERACT:
						if (input->IsAnyPressed(actionKeysPair.second))
						{
							Interact(entity, dtSinceLastFrame);
						}
						break;
					case ControlAction::INTERACT_ROTATE:
						rotating = true;
						break;
					case ControlAction::MOVE_CROUCH:
						if (noclip)
						{
							inputMovement += glm::vec3(0, -1, 0);
						}
						else
						{
							crouching = true;
						}
						break;
					default:
						std::stringstream ss;
						ss << "Unknown ControlAction: "
						   << static_cast<std::underlying_type<ControlAction>::type>(action);
						throw std::invalid_argument(ss.str());
				}
			}

			// Handle mouse controls
			auto transform = entity.Get<ecs::Transform>();
			auto controller = entity.Get<ecs::HumanController>();
			glm::vec2 dCursor = input->CursorDiff();

			if (!rotating || !InteractRotate(entity, dtSinceLastFrame, dCursor))
			{
				float sensitivity = CVarCursorSensitivity.Get() * 0.001;
				controller->yaw -= dCursor.x * sensitivity;
				if (controller->yaw > 2.0f * M_PI)
				{
					controller->yaw -= 2.0f * M_PI;
				}
				if (controller->yaw < 0)
				{
					controller->yaw += 2.0f * M_PI;
				}

				controller->pitch -= dCursor.y * sensitivity;

				const float feps = std::numeric_limits<float>::epsilon();
				controller->pitch = std::max(-((float)M_PI_2 - feps), std::min(controller->pitch, (float)M_PI_2 - feps));

				transform->SetRotate(glm::quat(glm::vec3(controller->pitch, controller->yaw, controller->roll)));
			}

			// Move the player
			if (CVarNoClip.Changed())
			{
				physics->ToggleCollisions(controller->pxController->getActor(), !CVarNoClip.Get(true));
			}

			auto currentHeight = physics->GetCapsuleHeight(controller->pxController);
			auto targetHeight = crouching ? ecs::PLAYER_CAPSULE_CROUCH_HEIGHT : ecs::PLAYER_CAPSULE_HEIGHT;
			if (fabs(targetHeight - currentHeight) > 0.1)
			{
				// If player is in the air, resize from the top to implement crouch-jumping.
				auto newHeight = currentHeight + (targetHeight - currentHeight) * (controller->onGround ? 0.1 : 1.0);
				ResizeEntity(entity, newHeight, !controller->onGround);
			}

			auto velocity = CalculatePlayerVelocity(entity, dtSinceLastFrame, inputMovement, jumping, sprinting, crouching);
			MoveEntity(entity, dtSinceLastFrame, velocity);
		}

		return true;
	}

	ecs::Handle<HumanController> HumanControlSystem::AssignController(ecs::Entity entity, sp::PhysxManager &px)
	{
		if (entity.Has<HumanController>())
		{
			std::stringstream ss;
			ss << "entity " << entity
			   << " cannot be assigned a new HumanController because it already has one.";
			throw std::invalid_argument(ss.str());
		}
		auto transform = entity.Get<ecs::Transform>();
		glm::quat rotation = transform->GetRotate();

		auto controller = entity.Assign<HumanController>();
		controller->SetRotate(rotation);
		controller->inputMap =
		{
			{
				ControlAction::MOVE_FORWARD, {GLFW_KEY_W}
			},
			{
				ControlAction::MOVE_BACKWARD, {GLFW_KEY_S}
			},
			{
				ControlAction::MOVE_LEFT, {GLFW_KEY_A}
			},
			{
				ControlAction::MOVE_RIGHT, {GLFW_KEY_D}
			},
			{
				ControlAction::MOVE_JUMP, {GLFW_KEY_SPACE}
			},
			{
				ControlAction::MOVE_CROUCH, {GLFW_KEY_LEFT_CONTROL}
			},
			{
				ControlAction::MOVE_SPRINT, {GLFW_KEY_LEFT_SHIFT}
			},
			{
				ControlAction::INTERACT, {GLFW_KEY_E}
			},
			{
				ControlAction::INTERACT_ROTATE, {GLFW_KEY_R}
			}
		};

		auto interact = entity.Assign<InteractController>();
		interact->manager = &px;

		// Offset the capsule position so the camera is at the top
		physx::PxVec3 pos = GlmVec3ToPxVec3(
			transform->GetPosition() - glm::vec3(0, ecs::PLAYER_CAPSULE_HEIGHT / 2, 0)
		);
		controller->pxController = px.CreateController(pos, ecs::PLAYER_RADIUS, ecs::PLAYER_CAPSULE_HEIGHT, 0.5f);
		controller->pxController->setStepOffset(ecs::PLAYER_STEP_HEIGHT);

		return controller;
	}

	void HumanControlSystem::Teleport(ecs::Entity entity, glm::vec3 position, glm::quat rotation)
	{
		if (!entity.Has<ecs::Transform>())
		{
			throw std::invalid_argument("entity must have a Transform component");
		}
		if (!entity.Has<ecs::HumanController>())
		{
			throw std::invalid_argument("entity must have a HumanController component");
		}

		auto controller = entity.Get<ecs::HumanController>();
		auto transform = entity.Get<ecs::Transform>();
		transform->SetPosition(position);
		if (rotation != glm::quat())
		{
			transform->SetRotate(rotation);
			controller->SetRotate(rotation);
		}

		if (controller->pxController)
		{
			// Offset the capsule position so the camera is at the top
			float capsuleHeight = physics->GetCapsuleHeight(controller->pxController);
			physics->TeleportController(
				controller->pxController,
				GlmVec3ToPxExtendedVec3(position - glm::vec3(0, capsuleHeight / 2, 0))
			);
		}
	}

	glm::vec3 HumanControlSystem::CalculatePlayerVelocity(ecs::Entity entity, double dtSinceLastFrame, glm::vec3 inDirection, bool jump, bool sprint, bool crouch)
	{
		if (!entity.Has<ecs::Transform>())
		{
			throw std::invalid_argument("entity must have a Transform component");
		}

		auto noclip = CVarNoClip.Get();
		auto controller = entity.Get<ecs::HumanController>();
		auto transform = entity.Get<ecs::Transform>();

		glm::vec3 movement = transform->GetRotate() * glm::vec3(inDirection.x, 0, inDirection.z);

		if (!noclip)
		{
			if (std::abs(movement.y) > 0.999)
			{
				movement = transform->GetRotate() * glm::vec3(0, -movement.y, 0);
			}
			movement.y = 0;
		}
		if (movement != glm::vec3(0))
		{
			float speed = CVarMovementSpeed.Get();
			if (sprint && controller->onGround) speed = CVarSprintSpeed.Get();
			if (crouch && controller->onGround) speed = CVarCrouchSpeed.Get();
			movement = glm::normalize(movement) * speed;
		}
		movement.y += inDirection.y * CVarMovementSpeed.Get();

		if (noclip)
		{
			controller->velocity = movement;
			return controller->velocity;
		}
		if (controller->onGround)
		{
			controller->velocity.x = movement.x;
			controller->velocity.y -= 0.01; // Always try moving down so that onGround detection is more consistent.
			if (jump) controller->velocity.y = ecs::PLAYER_JUMP_VELOCITY;
			controller->velocity.z = movement.z;
		}
		else
		{
			controller->velocity += movement * ecs::PLAYER_AIR_STRAFE * (float)dtSinceLastFrame;
			controller->velocity.y -= ecs::PLAYER_GRAVITY * dtSinceLastFrame;
		}

		return controller->velocity;
	}

	void HumanControlSystem::MoveEntity(ecs::Entity entity, double dtSinceLastFrame, glm::vec3 velocity)
	{
		auto transform = entity.Get<ecs::Transform>();
		auto controller = entity.Get<HumanController>();

		if (controller->pxController)
		{
			auto disp = velocity * (float)dtSinceLastFrame;
			auto prevPosition = PxExtendedVec3ToGlmVec3P(controller->pxController->getPosition());
			if (CVarNoClip.Get())
			{
				physics->TeleportController(controller->pxController, GlmVec3ToPxExtendedVec3(prevPosition + disp));
			}
			else
			{
				controller->onGround = physics->MoveController(controller->pxController, dtSinceLastFrame, GlmVec3ToPxVec3(disp));
			}
			auto newPosition = PxExtendedVec3ToGlmVec3P(controller->pxController->getPosition());
			// Don't accelerate more than our current velocity
			auto velocityPosition = glm::min(prevPosition + glm::abs(disp), newPosition);
			velocityPosition = glm::max(prevPosition - glm::abs(disp), velocityPosition);

			// Update the velocity based on what happened in physx
			controller->velocity = (velocityPosition - prevPosition) / (float)dtSinceLastFrame;
			glm::vec3 *velocity = (glm::vec3 *) controller->pxController->getUserData();
			*velocity = controller->velocity;

			// Offset the capsule position so the camera is at the top
			float capsuleHeight = physics->GetCapsuleHeight(controller->pxController);
			transform->SetPosition(newPosition + glm::vec3(0, capsuleHeight / 2, 0));
		}
	}

	bool HumanControlSystem::ResizeEntity(ecs::Entity entity, float height, bool fromTop)
	{
		auto transform = entity.Get<ecs::Transform>();
		auto controller = entity.Get<HumanController>();

		physx::PxCapsuleController *pxController = controller->pxController;
		if (pxController)
		{
			float oldHeight = physics->GetCapsuleHeight(pxController);
			physics->ResizeController(pxController, height, fromTop);

			physx::PxOverlapBuffer hit;
			physx::PxRigidDynamic* actor = pxController->getActor();

			bool valid = !physics->OverlapQuery(actor, physx::PxVec3(0), hit);

			if (!valid)
			{
				physics->ResizeController(pxController, oldHeight, fromTop);
			}
			return valid;
		}
		return false;
	}

	void HumanControlSystem::Interact(ecs::Entity entity, double dt)
	{
		auto interact = entity.Get<ecs::InteractController>();
		interact->PickUpObject(entity);
	}

	bool HumanControlSystem::InteractRotate(ecs::Entity entity, double dt, glm::vec2 dCursor)
	{
		auto interact = entity.Get<ecs::InteractController>();
		auto transform = entity.Get<ecs::Transform>();
		if (interact->target)
		{
			auto rotation = transform->GetRotate() * glm::vec3(dCursor.y, dCursor.x, 0) * (float)dt;
			rotation *= CVarCursorSensitivity.Get() * 10.0;
			physics->RotateConstraint(entity, interact->target, GlmVec3ToPxVec3(rotation));
			return true;
		}
		return false;
	}
}
