#pragma once

#include "ecs/Ecs.hh"
#include "ecs/components/Controller.hh"
#include "game/InputManager.hh"

#include <glm/glm.hpp>

namespace ECS
{
	class HumanControlSystem
	{
	public:
		HumanControlSystem(ECS::EntityManager *entities, sp::InputManager *input);
		~HumanControlSystem();

		/**
		 * Call this once per frame
		 */
		bool Frame(double dtSinceLastFrame);

		/**
		 * Assigns a default HumanController to the given entity.
		 */
		ECS::Handle<HumanController> AssignController(ECS::Entity entity);

	private:

		/**
		 * Move an entity in the given local direction based on how much time has passed
		 * since last frame.
		 */
		void move(ECS::Entity entity, double dt, glm::vec3 normalizedDirection);

		static const float MOVE_SPEED;
		static const glm::vec2 CURSOR_SENSITIVITY;

		ECS::EntityManager *entities;
		sp::InputManager *input;
	};
}