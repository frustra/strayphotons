#pragma once

#include <PxActor.h>
#include <PxRigidDynamic.h>

namespace ecs
{
	struct Physics
	{
		Physics() {}
		Physics(physx::PxRigidActor *actor) : actor(actor) 
		{
			if (actor)
			{
				dynamic = static_cast<physx::PxRigidDynamic *>(actor);
			}
		}

		physx::PxRigidActor *actor = nullptr;
		physx::PxRigidDynamic *dynamic = nullptr;
		bool needsTransformSync = true;
	};
}
