#pragma once

#include "Common.hh"

#include "physx/PhysxManager.hh"

#include <PxRigidDynamic.h>

namespace ecs
{
	struct InteractController
	{
		void PickUpObject(ecs::Entity entity);

		bool interacting = false;
		physx::PxRigidDynamic *object;
		sp::PhysxManager *manager;
	};
}
