#include "PhysicsQuerySystem.hh"

#include "ecs/Ecs.hh"
#include "ecs/EcsImpl.hh"
#include "physx/PhysxManager.hh"
#include "physx/PhysxUtils.hh"

#include <PxQueryReport.h>

namespace sp {
    using namespace physx;

    PhysicsQuerySystem::PhysicsQuerySystem(PhysxManager &manager) : manager(manager) {}

    void PhysicsQuerySystem::Frame(ecs::Lock<ecs::Read<ecs::Transform>, ecs::Write<ecs::PhysicsQuery>> lock) {
        for (auto &entity : lock.EntitiesWith<ecs::PhysicsQuery>()) {
            if (!entity.Has<ecs::PhysicsQuery, ecs::Transform>(lock)) continue;

            auto &query = entity.Get<ecs::PhysicsQuery>(lock);
            if (query.raycastQueryDistance > 0.0f) {
                auto transform = entity.Get<ecs::Transform>(lock).GetGlobalTransform(lock);

                PxFilterData filterData;
                filterData.word0 = (uint32_t)query.raycastQueryFilterGroup;

                glm::vec3 rayStart = transform.GetPosition();
                glm::vec3 rayDir = transform.GetForward();
                PxRaycastBuffer hit;
                bool status = manager.scene->raycast(GlmVec3ToPxVec3(rayStart),
                    GlmVec3ToPxVec3(rayDir),
                    query.raycastQueryDistance,
                    hit,
                    PxHitFlags(),
                    PxQueryFilterData(filterData, PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC));

                if (status) {
                    physx::PxRigidActor *hitActor = hit.block.actor;
                    if (hitActor) {
                        auto userData = (ActorUserData *)hitActor->userData;
                        if (userData) {
                            query.raycastHitTarget = userData->entity;
                            query.raycastHitPosition = rayDir * hit.block.distance + rayStart;
                            query.raycastHitDistance = hit.block.distance;
                            continue;
                        }
                    }
                }

                query.raycastHitTarget = Tecs::Entity();
                query.raycastHitPosition = glm::vec3();
                query.raycastHitDistance = 0.0f;
            }
        }
    }
} // namespace sp
