#pragma once

#include "physx/PhysxManager.hh"

#include <Ecs.hh>

namespace sp
{
	class InputManager;
}

namespace ecs
{
	class AnimationSystem
	{
	public:
		AnimationSystem(EntityManager &entities, sp::PhysxManager &physics);

		~AnimationSystem();

		bool Frame(float dtSinceLastFrame);

	private:

		EntityManager &entities;
		sp::PhysxManager &physics;
	};
}