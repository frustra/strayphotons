#pragma once

#include <glm/glm.hpp>
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
		Physics(physx::PxRigidActor *actor, shared_ptr<sp::Model> model) : actor(actor), model(model) {}

		physx::PxRigidActor *actor = nullptr;
		glm::vec3 scale = glm::vec3(1.0);
		shared_ptr<sp::Model> model;
	};
}
