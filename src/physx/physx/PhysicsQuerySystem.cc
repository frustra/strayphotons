#include "PhysicsQuerySystem.hh"

#include "ecs/Ecs.hh"
#include "ecs/EcsImpl.hh"
#include "physx/PhysxManager.hh"
#include "physx/PhysxUtils.hh"

#include <PxQueryReport.h>

namespace sp {
    using namespace physx;

    PhysicsQuerySystem::PhysicsQuerySystem(PhysxManager &manager) : manager(manager) {}

    void PhysicsQuerySystem::Frame(ecs::Lock<ecs::Read<ecs::TransformSnapshot>, ecs::Write<ecs::PhysicsQuery>> lock) {
        for (auto &entity : lock.EntitiesWith<ecs::PhysicsQuery>()) {
            auto &query = entity.Get<ecs::PhysicsQuery>(lock);
            for (auto &subQuery : query.queries) {
                std::visit(
                    [&](auto &&arg) {
                        using T = std::decay_t<decltype(arg)>;
                        arg.result.reset();

                        if constexpr (std::is_same_v<T, ecs::PhysicsQuery::Raycast>) {
                            if (arg.maxDistance > 0.0f && entity.Has<ecs::TransformSnapshot>(lock)) {
                                auto &transform = entity.Get<ecs::TransformSnapshot>(lock);

                                PxFilterData filterData;
                                filterData.word0 = (uint32_t)arg.filterGroup;

                                glm::vec3 rayStart = transform.GetPosition();
                                glm::vec3 rayDir = transform.GetForward();
                                PxRaycastBuffer hit;
                                bool status = manager.scene->raycast(GlmVec3ToPxVec3(rayStart),
                                    GlmVec3ToPxVec3(rayDir),
                                    arg.maxDistance,
                                    hit,
                                    PxHitFlags(),
                                    PxQueryFilterData(filterData, PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC));

                                if (status) {
                                    physx::PxRigidActor *hitActor = hit.block.actor;
                                    if (hitActor) {
                                        auto userData = (ActorUserData *)hitActor->userData;
                                        if (userData) {
                                            auto &result = arg.result.emplace();
                                            result.target = userData->entity;
                                            result.position = rayDir * hit.block.distance + rayStart;
                                            result.distance = hit.block.distance;
                                        }
                                    }
                                }
                            }
                        } else if constexpr (std::is_same_v<T, ecs::PhysicsQuery::Sweep>) {
                            if (arg.maxDistance > 0.0f && entity.Has<ecs::TransformSnapshot>(lock)) {
                                auto &transform = entity.Get<ecs::TransformSnapshot>(lock);

                                PxFilterData filterData;
                                filterData.word0 = (uint32_t)arg.filterGroup;

                                glm::vec3 rayStart = transform.GetPosition();
                                glm::vec3 rayDir = transform.GetForward();
                                PxSweepBuffer hit;
                                // GeometryFromShape
                                PxCapsuleGeometry capsuleGeometry(ecs::PLAYER_RADIUS, currentHeight * 0.5f);
                                auto sweepDist = targetHeight - currentHeight + contactOffset;
                                bool status = manager.scene->sweep(capsuleGeometry,
                                    actor->getGlobalPose(),
                                    PxVec3(0, 1, 0),
                                    sweepDist,
                                    hit,
                                    PxHitFlags(),
                                    PxQueryFilterData(filterData, PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC));

                                bool status = manager.scene->sweep(GlmVec3ToPxVec3(rayStart),
                                    GlmVec3ToPxVec3(rayDir),
                                    arg.maxDistance,
                                    hit,
                                    PxHitFlags(),
                                    PxQueryFilterData(filterData, PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC));

                                if (status) {
                                    physx::PxRigidActor *hitActor = hit.block.actor;
                                    if (hitActor) {
                                        auto userData = (ActorUserData *)hitActor->userData;
                                        if (userData) {
                                            auto &result = arg.result.emplace();
                                            result.target = userData->entity;
                                            result.position = rayDir * hit.block.distance + rayStart;
                                            result.distance = hit.block.distance;
                                        }
                                    }
                                }
                            }
                        } else if constexpr (std::is_same_v<T, ecs::PhysicsQuery::Overlap>) {
                        } else if constexpr (std::is_same_v<T, ecs::PhysicsQuery::Mass>) {
                            auto target = arg.targetActor.Get(lock);
                            if (target) {
                                if (manager.actors.count(target) > 0) {
                                    const auto &actor = manager.actors[target];
                                    auto dynamic = actor->is<PxRigidDynamic>();
                                    if (dynamic) {
                                        auto &result = arg.result.emplace();
                                        result.weight = dynamic->getMass();
                                        result.centerOfMass = PxVec3ToGlmVec3(dynamic->getCMassLocalPose().p);
                                    }
                                }
                            }
                        } else {
                            Errorf("Unknown PhysicsQuery type: %s", typeid(T).name());
                        }
                    },
                    subQuery);
            }
        }
    }
} // namespace sp
