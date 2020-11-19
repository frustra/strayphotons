#pragma once

#include "physx/PhysxActorDesc.hh"

#include <PxActor.h>
#include <PxRigidDynamic.h>
#include <ecs/Ecs.hh>
#include <ecs/NamedEntity.hh>
#include <glm/glm.hpp>

namespace sp {
	class Model;
}

namespace ecs {
	struct Physics {
		Physics() {}
		Physics(shared_ptr<sp::Model> model, sp::PhysxActorDesc desc) : model(model), desc(desc) {}
		Physics(physx::PxRigidActor *actor, shared_ptr<sp::Model> model, sp::PhysxActorDesc desc)
			: actor(actor), model(model), desc(desc) {}

		physx::PxRigidActor *actor = nullptr;
		shared_ptr<sp::Model> model;
		sp::PhysxActorDesc desc;

		glm::vec3 scale = glm::vec3(1.0);
	};

	static Component<Physics> ComponentPhysics("physics");

	template<>
	bool Component<Physics>::LoadEntity(Entity &dst, picojson::value &src);
} // namespace ecs
