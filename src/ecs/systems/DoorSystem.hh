#pragma once

#include <Ecs.hh>

namespace ecs
{
	class DoorSystem
	{
	public:
		DoorSystem(EntityManager &entities);
		bool Frame(float dtSinceLastFrame);

	private:
		EntityManager &entities;
	};
}