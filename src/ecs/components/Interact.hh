#pragma once

#include "Common.hh"

#include "physx/PhysxManager.hh"

#include <PxRigidDynamic.h>

namespace ecs
{
	struct InteractController
	{
		void PickUpObject(ecs::Entity entity);

		physx::PxRigidDynamic *target = nullptr;
		sp::PhysxManager *manager;
	};
}
