#pragma once

#include <Ecs.hh>
#include "ecs/components/Controller.hh"
#include "game/InputManager.hh"

#include "physx/PhysxManager.hh"

#include <glm/glm.hpp>

namespace ecs
{
	class HumanControlSystem
	{
	public:
		HumanControlSystem(ecs::EntityManager *entities, sp::InputManager *input, sp::PhysxManager *physics);
		~HumanControlSystem();

		/**
		 * Call this once per frame
		 */
		bool Frame(double dtSinceLastFrame);

		/**
		 * Assigns a default HumanController to the given entity.
		 */
		ecs::Handle<HumanController> AssignController(ecs::Entity entity, sp::PhysxManager &px);

	private:
		glm::vec3 CalculatePlayerVelocity(ecs::Entity entity, double dtSinceLastFrame, glm::vec3 inDirection, bool jump, bool sprint);
		void MoveEntity(ecs::Entity entity, double dtSinceLastFrame, glm::vec3 velocity);

		/**
		* Pick up the object that the player is looking at and make it move at to a fixed location relative to camera
		*/
		void Interact(ecs::Entity entity, double dt);

		ecs::EntityManager *entities;
		sp::InputManager *input;
		sp::PhysxManager *physics;

	};
}