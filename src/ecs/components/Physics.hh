#pragma once

#include <PxActor.h>
#include <PxRigidDynamic.h>

namespace sp
{
	class Model;
}

namespace ecs
{
	struct Physics
	{
		Physics() {}
		Physics(physx::PxRigidActor *actor, shared_ptr<sp::Model> model) : actor(actor), model(model)
		{
			if (actor)
			{
				dynamic = static_cast<physx::PxRigidDynamic *>(actor);
			}
		}

		physx::PxRigidActor *actor = nullptr;
		physx::PxRigidDynamic *dynamic = nullptr;
		shared_ptr<sp::Model> model;
		bool needsTransformSync = true;
	};
}
