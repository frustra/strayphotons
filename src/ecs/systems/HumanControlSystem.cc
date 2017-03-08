#define _USE_MATH_DEFINES
#include <cmath>

#include "ecs/systems/HumanControlSystem.hh"
#include "ecs/components/Interact.hh"
#include "ecs/components/Transform.hh"
#include "ecs/components/Physics.hh"

#include "physx/PhysxUtils.hh"

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
	static sp::CVar<float> CVarMovementSpeed("p.MovementSpeed", 3.0, "Player movement speed (m/s)");
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
			// Handle mouse controls
			auto transform = entity.Get<ecs::Transform>();
			auto controller = entity.Get<ecs::HumanController>();
			glm::vec2 dCursor = input->CursorDiff();

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

			transform->rotate = glm::quat(glm::vec3(controller->pitch, controller->yaw, controller->roll));

			// Handle keyboard controls
			auto noclip = CVarNoClip.Get();
			glm::vec3 inputMovement = glm::vec3(0);
			bool jumping = false;

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
					case ControlAction::MOVE_CROUCH:
						if (noclip)
						{
							inputMovement += glm::vec3(0, -1, 0);
						}
						break;
					case ControlAction::INTERACT:
						if (input->IsAnyPressed(actionKeysPair.second))
						{
							Interact(entity, dtSinceLastFrame);
						}
						break;
					default:
						std::stringstream ss;
						ss << "Unknown ControlAction: "
						   << static_cast<std::underlying_type<ControlAction>::type>(action);
						throw std::invalid_argument(ss.str());
				}
			}

			if (CVarNoClip.Changed())
			{
				physics->ToggleCollisions(controller->pxController->getActor(), !CVarNoClip.Get(true));
			}

			bool onGround = physics->SweepQuery(controller->pxController->getActor(), physx::PxVec3(0, -1, 0), ecs::PLAYER_SWEEP_DISTANCE);
			auto velocity = CalculatePlayerVelocity(entity, dtSinceLastFrame, onGround, inputMovement, jumping);
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
		auto controller = entity.Assign<HumanController>();
		controller->pitch = 0;
		controller->yaw = 0;
		controller->roll = 0;
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
				ControlAction::INTERACT, {GLFW_KEY_E}
			}
		};

		auto interact = entity.Assign<InteractController>();
		interact->manager = &px;

		auto transform = entity.Get<ecs::Transform>();
		// Offset the capsule position so the camera is at the top
		physx::PxVec3 pos = GlmVec3ToPxVec3(transform->GetPosition() - glm::vec3(0, ecs::PLAYER_HEIGHT / 2, 0));
		controller->pxController = px.CreateController(pos, ecs::PLAYER_RADIUS, ecs::PLAYER_HEIGHT - ecs::PLAYER_RADIUS * 2.0, 0.5f);
		controller->pxController->setStepOffset(ecs::PLAYER_STEP_HEIGHT);

		return controller;
	}

	glm::vec3 HumanControlSystem::CalculatePlayerVelocity(ecs::Entity entity, double dtSinceLastFrame, bool onGround, glm::vec3 inDirection, bool jump)
	{
		if (!entity.Has<ecs::Transform>())
		{
			throw std::invalid_argument("entity must have a Transform component");
		}

		auto noclip = CVarNoClip.Get();
		auto controller = entity.Get<ecs::HumanController>();
		auto transform = entity.Get<ecs::Transform>();

		glm::vec3 movement = transform->rotate * glm::vec3(inDirection.x, 0, inDirection.z);

		if (!noclip)
		{
			if (std::abs(movement.y) > 0.999)
			{
				movement = transform->rotate * glm::vec3(0, -movement.y, 0);
			}
			movement.y = 0;
		}
		if (movement != glm::vec3(0)) movement = glm::normalize(movement) * CVarMovementSpeed.Get();
		movement.y += inDirection.y * CVarMovementSpeed.Get();

		if (onGround || noclip)
		{
			if (jump) movement.y += ecs::PLAYER_JUMP_VELOCITY;
			controller->velocity = movement;
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
				physics->MoveController(controller->pxController, dtSinceLastFrame, GlmVec3ToPxVec3(disp));
			}
			auto newPosition = PxExtendedVec3ToGlmVec3P(controller->pxController->getPosition());
			// Don't accelerate from physics glitches
			newPosition = glm::min(prevPosition + glm::abs(disp), newPosition);
			newPosition = glm::max(prevPosition - glm::abs(disp), newPosition);

			// Update the velocity based on what happened in physx
			controller->velocity = (newPosition - prevPosition) / (float)dtSinceLastFrame;

			// Offset the capsule position so the camera is at the top
			transform->SetPosition(newPosition + glm::vec3(0, PLAYER_HEIGHT / 2, 0));
		}
	}

	void HumanControlSystem::Interact(ecs::Entity entity, double dt)
	{
		auto interact = entity.Get<ecs::InteractController>();
		interact->PickUpObject(entity);
	}
}
