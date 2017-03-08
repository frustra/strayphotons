#include <Ecs.hh>
#include <glm/glm.hpp>

#include "ecs/components/Controller.hh"
#include "ecs/components/Interact.hh"
#include "ecs/components/Transform.hh"

#include "physx/PhysxUtils.hh"

namespace ecs
{
	void InteractController::PickUpObject(ecs::Entity entity)
	{
		if (target)
		{
			target->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, false);
			manager->RemoveConstraints(entity, target);
			target = nullptr;
			return;
		}

		auto transform = entity.Get<ecs::Transform>();
		auto controller = entity.Get<ecs::HumanController>();

		physx::PxVec3 origin = GlmVec3ToPxVec3(transform->GetPosition());

		glm::vec3 forward = glm::vec3(0, 0, -1);
		glm::vec3 rotate = transform->rotate * forward;

		physx::PxVec3 dir = GlmVec3ToPxVec3(rotate);
		dir.normalizeSafe();
		physx::PxReal maxDistance = 2.0f;

		physx::PxRaycastBuffer hit;
		bool status = manager->RaycastQuery(entity, origin, dir, maxDistance, hit);

		if (status)
		{
			physx::PxRigidActor *hitActor = hit.block.actor;
			if (hitActor && hitActor->getType() == physx::PxActorType::eRIGID_DYNAMIC)
			{
				physx::PxRigidDynamic *dynamic = static_cast<physx::PxRigidDynamic *>(hitActor);
				if (dynamic && !(dynamic->getRigidBodyFlags() & physx::PxRigidBodyFlag::eKINEMATIC))
				{
					target = dynamic;
					dynamic->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, true);
					manager->CreateConstraint(entity, dynamic, physx::PxVec3(hit.block.distance, 0.f, 0.f));
				}
			}
		}
	}
}