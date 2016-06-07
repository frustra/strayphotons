#pragma once

#include <PxActor.h>	

namespace ECS
{
	struct Physics
	{
		Physics() {}
		Physics(physx::PxActor* actor) : actor(actor) {}
		physx::PxActor* actor;
	};
}
