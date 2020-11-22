#include "ecs/components/Interact.hh"

#include "physx/PhysxUtils.hh"

#include <ecs/Ecs.hh>
#include <ecs/EcsImpl.hh>
#include <glm/glm.hpp>

namespace ecs {
    void InteractController::PickUpObject(ecs::Entity entity) {
        if (target) {
            manager->RemoveConstraint(entity, target);
            target = nullptr;
            return;
        }

        auto transform = entity.Get<ecs::Transform>();

        physx::PxVec3 origin = GlmVec3ToPxVec3(transform->GetPosition());

        glm::vec3 rotate = transform->GetForward();

        physx::PxVec3 dir = GlmVec3ToPxVec3(rotate);
        dir.normalizeSafe();
        physx::PxReal maxDistance = 2.0f;

        physx::PxRaycastBuffer hit;
        bool status = manager->RaycastQuery(entity, origin, dir, maxDistance, hit);

        if (status) {
            physx::PxRigidActor *hitActor = hit.block.actor;
            if (hitActor && hitActor->getType() == physx::PxActorType::eRIGID_DYNAMIC) {
                physx::PxRigidDynamic *dynamic = static_cast<physx::PxRigidDynamic *>(hitActor);
                if (dynamic && !dynamic->getRigidBodyFlags().isSet(physx::PxRigidBodyFlag::eKINEMATIC)) {
                    target = dynamic;
                    auto pose = dynamic->getGlobalPose();
                    auto currentPos = pose.transform(dynamic->getCMassLocalPose().transform(physx::PxVec3(0.0)));
                    auto invRotate = glm::inverse(transform->GetRotate());
                    auto offset = invRotate * (PxVec3ToGlmVec3P(currentPos - origin) + glm::vec3(0, 0.1, 0));
                    manager->CreateConstraint(entity,
                                              dynamic,
                                              GlmVec3ToPxVec3(offset),
                                              GlmQuatToPxQuat(invRotate) * pose.q);
                }
            }
        }
    }
} // namespace ecs
