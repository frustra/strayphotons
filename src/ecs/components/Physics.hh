#pragma once

#include <PxActor.h>	

namespace ecs
{
	struct Physics
	{
		Physics() {}
		Physics(physx::PxRigidActor* actor) : actor(actor) {}
		physx::PxRigidActor* actor;
	};
}
