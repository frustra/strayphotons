#pragma once

#include "physx/PhysxManager.hh"

#include <Ecs.hh>

namespace sp
{
	class InputManager;
}

namespace ecs
{
	class AnimateBlockSystem
	{
	public:
		AnimateBlockSystem(EntityManager &entities, sp::PhysxManager &physics);

		~AnimateBlockSystem();

		bool Frame(float dtSinceLastFrame);

	private:

		EntityManager &entities;
		sp::PhysxManager &physics;
	};
}