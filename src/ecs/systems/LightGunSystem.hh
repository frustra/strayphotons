#pragma once

#include "physx/PhysxManager.hh"

#include <Ecs.hh>

namespace sp
{
	class InputManager;
	class GameLogic;
}

namespace ecs
{
	class LightGunSystem
	{
	public:
		LightGunSystem(EntityManager *entities, sp::PhysxManager *physics, sp::GameLogic *logic);

		~LightGunSystem();

		void BindInput(sp::InputManager *inputManager)
		{
			input = inputManager;
		}

		bool Frame(float dtSinceLastFrame);
		void SuckLight(Entity &gun);
		void ShootLight(Entity &gun);

	private:
		Entity EntityRaycast(Entity &origin);

		EntityManager *entities;
		sp::InputManager *input;
		sp::PhysxManager *physics;
		sp::GameLogic *logic;
	};
}