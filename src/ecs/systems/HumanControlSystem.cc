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

	const float HumanControlSystem::MOVE_SPEED = 5.5f;
	const glm::vec2 HumanControlSystem::CURSOR_SENSITIVITY = glm::vec2(0.001f, 0.001f);

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

			if(!controller->jumping)
			{
				controller->lastGroundVelocity = glm::vec3();
			}

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
						move(entity, dtSinceLastFrame, glm::vec3(0, 0, -1));
						break;
					case ControlAction::MOVE_BACKWARD:
						move(entity, dtSinceLastFrame, glm::vec3(0, 0, 1));
						break;
					case ControlAction::MOVE_LEFT:
						move(entity, dtSinceLastFrame, glm::vec3(-1, 0, 0));
						break;
					case ControlAction::MOVE_RIGHT:
						move(entity, dtSinceLastFrame, glm::vec3(1, 0, 0));
						break;
					case ControlAction::MOVE_UP:
						move(entity, dtSinceLastFrame, glm::vec3(0, 1, 0), true);
						break;
					case ControlAction::MOVE_DOWN:
						move(entity, dtSinceLastFrame, glm::vec3(0, -1, 0), true);
						break;
					case ControlAction::MOVE_JUMP:
						if (!controller->jumping)
						{
							controller->upVelocity = ecs::CONTROLLER_JUMP;
						}
						break;
					case ControlAction::INTERACT:
						if(input->IsAnyPressed(actionKeysPair.second))
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

			controller->jumping = !physics->SweepQuery(controller->pxController->getActor(), physx::PxVec3(0,-1,0), ecs::CONTROLLER_SWEEP_DISTANCE);

			if (controller->jumping)
			{
				controllerMove(entity, dtSinceLastFrame, (controller->lastGroundVelocity * (float)dtSinceLastFrame));
			}

			if (CVarNoClip.Changed() && !CVarNoClip.Get(true))
			{
				// Teleport character controller to position when disabling noclip
				auto pos = transform->GetPosition();
				controller->pxController->setPosition({pos[0], pos[1], pos[2]});
			}

			if (controller->pxController && !CVarNoClip.Get())
			{
				float jump = controller->upVelocity * dtSinceLastFrame;
				controller->upVelocity -= ecs::CONTROLLER_GRAVITY * dtSinceLastFrame;
				if (!controller->jumping) controller->upVelocity = 0.0; // Reset the velocity when we hit something.
				controllerMove(entity, dtSinceLastFrame, glm::vec3(0, jump, 0));

				auto position = controller->pxController->getPosition();
				physx::PxVec3 pxVec3 = physx::PxVec3(position.x, position.y, position.z);
				transform->SetPosition(PxVec3ToGlmVec3P(pxVec3));
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
				ControlAction::MOVE_UP, {GLFW_KEY_Z}
			},
			{
				ControlAction::MOVE_DOWN, {GLFW_KEY_X}
			},
			{
				ControlAction::MOVE_JUMP, {GLFW_KEY_SPACE}
			},
			{
				ControlAction::INTERACT, {GLFW_KEY_R}
			}
		};

		auto interact = entity.Assign<InteractController>();
		interact->manager = &px;

		auto transform = entity.Get<ecs::Transform>();
		physx::PxVec3 pos = GlmVec3ToPxVec3(transform->GetPosition());
		controller->pxController = px.CreateController(pos, 0.2f, 1.5f, 0.5f);
		controller->pxController->setStepOffset(ecs::CONTROLLER_STEP);

		return controller;
	}

	void HumanControlSystem::move(ecs::Entity entity, double dt, glm::vec3 normalizedDirection, bool flight)
	{
		if (!entity.Has<ecs::Transform>())
		{
			throw std::invalid_argument("entity must have a Transform component");
		}

		auto controller = entity.Get<ecs::HumanController>();
		auto transform = entity.Get<ecs::Transform>();

		float df = (float)dt;

		glm::vec3 movement;
		glm::vec3 deltaMove;

		if (flight)
		{
			movement = normalizedDirection * df;
		}
		else
		{
			movement = transform->rotate * normalizedDirection;
			if (std::abs(movement.y) > 0.999)
			{
				movement = transform->rotate * glm::vec3(0, -movement.y, 0);
			}
			movement.y = 0;
			movement = glm::normalize(movement) * HumanControlSystem::MOVE_SPEED;
			deltaMove = movement * df;
		}
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
