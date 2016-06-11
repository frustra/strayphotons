#include "ecs/systems/HumanControlSystem.hh"
#include "ecs/components/Transform.hh"

#include "Common.hh"

#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

#include <sstream>

namespace ECS
{
	const float HumanControlSystem::MOVE_SPEED = 6.0f;
	const glm::vec2 HumanControlSystem::CURSOR_SENSITIVITY = glm::vec2(0.007f, 0.007f);

	HumanControlSystem::HumanControlSystem(sp::EntityManager *entities, sp::InputManager *input)
		: entities(entities), input(input)
	{
	}

	HumanControlSystem::~HumanControlSystem()
	{
	}

	bool HumanControlSystem::Frame(double dtSinceLastFrame)
	{
		for (sp::Entity entity : entities->EntitiesWith<ECS::Transform, ECS::HumanController>())
		{
			// control orientation with the mouse
			auto *transform = entity.Get<ECS::Transform>();
			auto *controller = entity.Get<ECS::HumanController>();


			glm::vec2 dCursor = input->CursorDiff();

			controller->yaw   -= dCursor.x*CURSOR_SENSITIVITY.x;
			if (controller->yaw > 2*M_PI)
			{
				controller->yaw -= 2*M_PI;
			}
			if (controller->yaw < 0)
			{
				controller->yaw += 2*M_PI;
			}

			controller->pitch -= dCursor.y*CURSOR_SENSITIVITY.y;

			const float feps = std::numeric_limits<float>::epsilon();
			controller->pitch = std::max(-((float)M_PI_2 - feps), std::min(controller->pitch, (float)M_PI_2 - feps));

			transform->rotate = glm::quat(glm::vec3(controller->pitch, controller->yaw, controller->roll));

			// keyboard controls
			for (auto const &actionKeysPair : entity.Get<ECS::HumanController>()->inputMap)
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
					move(entity, dtSinceLastFrame, glm::vec3(0, 1, 0));
					break;
				case ControlAction::MOVE_DOWN:
					move(entity, dtSinceLastFrame, glm::vec3(0, -1, 0));
					break;
				default:
					std::stringstream ss;
					ss << "Unknown ControlAction: "
					   << static_cast<std::underlying_type<ControlAction>::type>(action);
					throw std::invalid_argument(ss.str());
				}
			}
		}

		return true;
	}

	HumanController *HumanControlSystem::AssignController(sp::Entity entity)
	{
		if (entity.Has<HumanController>())
		{
			std::stringstream ss;
			ss << "entity " << entity
			   << " cannot be assigned a new HumanController because it already has one.";
			throw std::invalid_argument(ss.str());
		}
		auto *controller = entity.Assign<HumanController>();
		controller->pitch = 0;
		controller->yaw = 0;
		controller->roll = 0;
		controller->inputMap = {
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
				ControlAction::MOVE_UP, {GLFW_KEY_SPACE}
			},
			{
				ControlAction::MOVE_DOWN, {GLFW_KEY_LEFT_CONTROL}
			},
		};

		return controller;
	}

	void HumanControlSystem::move(sp::Entity entity, double dt, glm::vec3 normalizedDirection)
	{
		if (!entity.Has<ECS::Transform>())
		{
			throw std::invalid_argument("entity must have a Transform component");
		}

		auto *transform = entity.Get<ECS::Transform>();
		float movement = HumanControlSystem::MOVE_SPEED * (float)dt;
		glm::vec4 translation(movement * normalizedDirection, 0);

		transform->Translate(transform->rotate * translation);
	}
}
