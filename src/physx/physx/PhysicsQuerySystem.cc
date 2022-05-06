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
        ZoneScoped;
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

                                auto shapeTransform = transform * arg.shape.transform;
                                PxTransform pxTransform(GlmVec3ToPxVec3(shapeTransform.GetPosition()),
                                    GlmQuatToPxQuat(shapeTransform.GetRotation()));
                                glm::vec3 sweepDir = transform * glm::vec4(arg.sweepDirection, 0.0f);

                                PxSweepBuffer hit;
                                bool status = manager.scene->sweep(manager.GeometryFromShape(arg.shape).any(),
                                    pxTransform,
                                    GlmVec3ToPxVec3(sweepDir),
                                    arg.maxDistance,
                                    hit,
                                    PxHitFlag::ePOSITION,
                                    PxQueryFilterData(filterData, PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC));

                                if (status) {
                                    physx::PxRigidActor *hitActor = hit.block.actor;
                                    if (hitActor) {
                                        auto userData = (ActorUserData *)hitActor->userData;
                                        if (userData) {
                                            auto &result = arg.result.emplace();
                                            result.target = userData->entity;
                                            result.position = PxVec3ToGlmVec3(hit.block.position);
                                            result.distance = hit.block.distance;
                                        }
                                    }
                                }
                            }
                        } else if constexpr (std::is_same_v<T, ecs::PhysicsQuery::Overlap>) {
                            if (entity.Has<ecs::TransformSnapshot>(lock)) {
                                auto &transform = entity.Get<ecs::TransformSnapshot>(lock);

                                PxFilterData filterData;
                                filterData.word0 = (uint32_t)arg.filterGroup;

                                auto shapeTransform = transform * arg.shape.transform;
                                PxTransform pxTransform(GlmVec3ToPxVec3(shapeTransform.GetPosition()),
                                    GlmQuatToPxQuat(shapeTransform.GetRotation()));

                                PxOverlapHit touch;
                                PxOverlapBuffer hit;
                                hit.touches = &touch;
                                hit.maxNbTouches = 1;
                                bool status = manager.scene->overlap(manager.GeometryFromShape(arg.shape).any(),
                                    pxTransform,
                                    hit,
                                    PxQueryFilterData(filterData, PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC));

                                if (status) {
                                    physx::PxRigidActor *hitActor = touch.actor;
                                    if (hitActor) {
                                        auto userData = (ActorUserData *)hitActor->userData;
                                        if (userData) arg.result.emplace(userData->entity);
                                    }
                                }
                            }
                        } else if constexpr (std::is_same_v<T, ecs::PhysicsQuery::Mass>) {
                            auto target = arg.targetActor.Get(lock);
                            if (target) {
                                if (manager.actors.count(target) > 0) {
                                    const auto &actor = manager.actors[target];
                                    auto dynamic = actor->template is<PxRigidDynamic>();
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
