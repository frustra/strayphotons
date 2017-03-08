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

	const float MOVE_SPEED = 3.0; // Unit is meters per second
	const glm::vec2 CURSOR_SENSITIVITY = glm::vec2(0.001, 0.001);

	const float PLAYER_HEIGHT = 1.3; // Add radius * 2 for total capsule height
	const float PLAYER_RADIUS = 0.2;

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
			// control orientation with the mouse
			auto transform = entity.Get<ecs::Transform>();
			auto controller = entity.Get<ecs::HumanController>();
			glm::vec2 dCursor = input->CursorDiff();

			controller->yaw   -= dCursor.x * CURSOR_SENSITIVITY.x;
			if (controller->yaw > 2.0f * M_PI)
			{
				controller->yaw -= 2.0f * M_PI;
			}
			if (controller->yaw < 0)
			{
				controller->yaw += 2.0f * M_PI;
			}

			controller->pitch -= dCursor.y * CURSOR_SENSITIVITY.y;

			const float feps = std::numeric_limits<float>::epsilon();
			controller->pitch = std::max(-((float)M_PI_2 - feps), std::min(controller->pitch, (float)M_PI_2 - feps));

			transform->rotate = glm::quat(glm::vec3(controller->pitch, controller->yaw, controller->roll));

			if (!controller->jumping)
			{
				controller->lastGroundVelocity = glm::vec3(0);
			}

			if (CVarNoClip.Changed() && !CVarNoClip.Get(true))
			{
				// Teleport character controller to position when disabling noclip
				auto pos = transform->GetPosition() - glm::vec3(0, PLAYER_HEIGHT / 2, 0);
				controller->pxController->setPosition({pos[0], pos[1], pos[2]});
			}
			auto noclip = CVarNoClip.Get();
			glm::vec3 inputMovement = glm::vec3(0);

			// keyboard controls
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
							move(entity, dtSinceLastFrame, glm::vec3(0, 1, 0), true);
						}
						else
						{
							if (!controller->jumping)
							{
								controller->upVelocity = ecs::CONTROLLER_JUMP;
							}
						}
						break;
					case ControlAction::MOVE_CROUCH:
						if (noclip)
						{
							move(entity, dtSinceLastFrame, glm::vec3(0, -1, 0), true);
						}
						break;
					case ControlAction::INTERACT:
						if (input->IsAnyPressed(actionKeysPair.second))
						{
							interact(entity, dtSinceLastFrame);
						}
						break;
					default:
						std::stringstream ss;
						ss << "Unknown ControlAction: "
						   << static_cast<std::underlying_type<ControlAction>::type>(action);
						throw std::invalid_argument(ss.str());
				}
			}

			move(entity, dtSinceLastFrame, inputMovement);

			controller->jumping = !noclip && !physics->SweepQuery(controller->pxController->getActor(), physx::PxVec3(0, -1, 0), ecs::CONTROLLER_SWEEP_DISTANCE);

			if (controller->jumping && !noclip)
			{
				controllerMove(entity, dtSinceLastFrame, (controller->lastGroundVelocity * (float)dtSinceLastFrame));
			}

			if (controller->pxController && !noclip)
			{
				float jump = controller->upVelocity * dtSinceLastFrame;
				controller->upVelocity -= ecs::CONTROLLER_GRAVITY * dtSinceLastFrame;
				if (!controller->jumping) controller->upVelocity = 0.0; // Reset the velocity when we hit something.
				controllerMove(entity, dtSinceLastFrame, glm::vec3(0, jump, 0));

				auto position = controller->pxController->getPosition();
				physx::PxVec3 pxVec3 = physx::PxVec3(position.x, position.y, position.z);
				transform->SetPosition(PxVec3ToGlmVec3P(pxVec3) + glm::vec3(0, PLAYER_HEIGHT / 2, 0));
			}
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
		physx::PxVec3 pos = GlmVec3ToPxVec3(transform->GetPosition() - glm::vec3(0, PLAYER_HEIGHT / 2, 0));
		controller->pxController = px.CreateController(pos, PLAYER_RADIUS, PLAYER_HEIGHT, 0.5f);
		controller->pxController->setStepOffset(ecs::CONTROLLER_STEP);

		return controller;
	}

	void HumanControlSystem::move(ecs::Entity entity, double dt, glm::vec3 direction, bool flight)
	{
		if (!entity.Has<ecs::Transform>())
		{
			throw std::invalid_argument("entity must have a Transform component");
		}
		if (direction == glm::vec3(0)) return;

		auto controller = entity.Get<ecs::HumanController>();
		auto transform = entity.Get<ecs::Transform>();

		glm::vec3 movement;
		glm::vec3 deltaMove;

		if (flight)
		{
			movement = direction;
		}
		else
		{
			movement = transform->rotate * direction;
			if (!CVarNoClip.Get())
			{
				if (std::abs(movement.y) > 0.999)
				{
					movement = transform->rotate * glm::vec3(0, -movement.y, 0);
				}
				movement.y = 0;
			}
		}
		movement = glm::normalize(movement) * MOVE_SPEED;
		deltaMove = movement * (float) dt;
		if (controller->jumping)
		{
			deltaMove *= ecs::CONTROLLER_AIR_STRAFE;
		}
		else
		{
			controller->lastGroundVelocity += movement;
		}
		controllerMove(entity, dt, deltaMove);
	}

	void HumanControlSystem::controllerMove(ecs::Entity entity, double dt, glm::vec3 movement)
	{
		auto transform = entity.Get<ecs::Transform>();
		auto controller = entity.Get<HumanController>();

		if (controller->pxController && !CVarNoClip.Get())
		{
			physx::PxControllerFilters filters;
			physx::PxScene *scene = controller->pxController->getScene();

			Assert(scene);
			scene->lockRead();
			controller->pxController->move(GlmVec3ToPxVec3(movement), 0, dt, filters);
			scene->unlockRead();
		}
		else
		{
			transform->Translate(movement);
		}
	}

	void HumanControlSystem::interact(ecs::Entity entity, double dt)
	{
		auto interact = entity.Get<ecs::InteractController>();
		interact->PickUpObject(entity);
	}
}
