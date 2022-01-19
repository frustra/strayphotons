#include "PhysicsQuerySystem.hh"

#include "ecs/Ecs.hh"
#include "ecs/EcsImpl.hh"
#include "physx/PhysxManager.hh"
#include "physx/PhysxUtils.hh"

#include <PxQueryReport.h>

namespace sp {
    using namespace physx;

    PhysicsQuerySystem::PhysicsQuerySystem(PhysxManager &manager) : manager(manager) {}

    void PhysicsQuerySystem::Frame() {
        auto lock = ecs::World.StartTransaction<ecs::Read<ecs::Transform>, ecs::Write<ecs::PhysicsQuery>>();

        for (auto &entity : lock.EntitiesWith<ecs::PhysicsQuery>()) {
            if (!entity.Has<ecs::PhysicsQuery, ecs::Transform>(lock)) continue;

            auto &query = entity.Get<ecs::PhysicsQuery>(lock);
            if (query.maxRaycastDistance > 0.0f) {
                auto transform = entity.Get<ecs::Transform>(lock).GetGlobalTransform(lock);

                PxFilterData filterData;
                filterData.word0 = PhysxCollisionGroup::WORLD;

                PxRaycastBuffer hit;
                bool status = manager.scene->raycast(GlmVec3ToPxVec3(transform.GetPosition()),
                    GlmVec3ToPxVec3(transform.GetForward()),
                    query.maxRaycastDistance,
                    hit,
                    PxHitFlags(PxEmpty), // TODO: See if anything is required from PxHitFlags::eDEFAULT
                    PxQueryFilterData(filterData,
                        PxQueryFlag::eANY_HIT | PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC));

                if (status) {
                    physx::PxRigidActor *hitActor = hit.block.actor;
                    if (hitActor) {
                        auto userData = (ActorUserData *)hitActor->userData;
                        if (userData) {
                            query.raycastHitTarget = userData->entity;
                            query.raycastHitPosition = PxVec3ToGlmVec3(hit.block.position);
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
