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
				if (dynamic && !dynamic->getRigidBodyFlags().isSet(physx::PxRigidBodyFlag::eKINEMATIC))
				{
					target = dynamic;
					auto currentPos = dynamic->getGlobalPose().transform(dynamic->getCMassLocalPose().transform(physx::PxVec3(0.0)));
					auto offset = glm::inverse(transform->rotate) * PxVec3ToGlmVec3P(currentPos - origin);
					manager->CreateConstraint(entity, dynamic, GlmVec3ToPxVec3(offset));
				}
			}
		}
	}
}