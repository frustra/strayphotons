/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

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
                        if constexpr (!std::is_same<T, std::monostate>()) arg.result.reset();

                        if constexpr (std::is_same<T, ecs::PhysicsQuery::Raycast>()) {
                            if (arg.maxDistance > 0.0f && arg.maxHits > 0) {
                                glm::vec3 rayStart = arg.position;
                                glm::vec3 rayDir = arg.direction;

                                if ((arg.relativePosition || arg.relativeDirection) &&
                                    entity.Has<ecs::TransformSnapshot>(lock)) {
                                    auto &transform = entity.Get<ecs::TransformSnapshot>(lock).globalPose;
                                    if (arg.relativePosition) rayStart = transform * glm::vec4(rayStart, 1);
                                    if (arg.relativeDirection) rayDir = transform * glm::vec4(rayDir, 0);
                                }

                                rayDir = glm::normalize(rayDir);

                                PxFilterData filterData;
                                filterData.word0 = (uint32_t)arg.filterGroup;

                                std::array<PxRaycastHit, 16> touches;

                                PxRaycastBuffer hit;
                                hit.touches = touches.data();

                                auto queryFlags = PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC;

                                if (arg.maxHits == 1) {
                                    hit.maxNbTouches = 0;
                                } else {
                                    hit.maxNbTouches = std::min((uint32)touches.size(), arg.maxHits);
                                }

                                manager.scene->raycast(GlmVec3ToPxVec3(rayStart),
                                    GlmVec3ToPxVec3(rayDir),
                                    arg.maxDistance,
                                    hit,
                                    PxHitFlag::eDEFAULT,
                                    PxQueryFilterData(filterData, queryFlags));

                                auto &result = arg.result.emplace();
                                result.hits = hit.getNbAnyHits();
                                result.position = PxVec3ToGlmVec3(hit.block.position);
                                result.normal = PxVec3ToGlmVec3(hit.block.normal);
                                result.distance = hit.block.distance;

                                physx::PxRigidActor *hitActor = nullptr;
                                physx::PxShape *hitShape = nullptr;
                                if (arg.maxHits == 1) {
                                    hitActor = hit.block.actor;
                                    hitShape = hit.block.shape;
                                } else if (result.hits > 0) {
                                    hitActor = hit.getTouch(0).actor;
                                    hitShape = hit.getTouch(0).shape;
                                }
                                if (hitShape) {
                                    auto userData = (ShapeUserData *)hitShape->userData;
                                    if (userData) {
                                        result.target = userData->parent;
                                        result.subTarget = userData->owner;
                                    }
                                } else if (hitActor) {
                                    auto userData = (ActorUserData *)hitActor->userData;
                                    if (userData) {
                                        result.target = userData->entity;
                                        result.subTarget = userData->entity;
                                    }
                                }
                            }
                        } else if constexpr (std::is_same<T, ecs::PhysicsQuery::Sweep>()) {
                            if (arg.maxDistance > 0.0f && entity.Has<ecs::TransformSnapshot>(lock)) {
                                auto &transform = entity.Get<ecs::TransformSnapshot>(lock).globalPose;

                                PxFilterData filterData;
                                filterData.word0 = (uint32_t)arg.filterGroup;

                                auto shapeTransform = transform * arg.shape.transform;
                                PxTransform pxTransform(GlmVec3ToPxVec3(shapeTransform.GetPosition()),
                                    GlmQuatToPxQuat(shapeTransform.GetRotation()));
                                glm::vec3 sweepDir = transform * glm::vec4(arg.sweepDirection, 0.0f);

                                PxSweepBuffer hit;
                                manager.scene->sweep(manager.GeometryFromShape(arg.shape).any(),
                                    pxTransform,
                                    GlmVec3ToPxVec3(sweepDir),
                                    arg.maxDistance,
                                    hit,
                                    PxHitFlag::ePOSITION,
                                    PxQueryFilterData(filterData, PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC));

                                auto &result = arg.result.emplace();

                                physx::PxRigidActor *hitActor = hit.block.actor;
                                if (hitActor) {
                                    auto userData = (ActorUserData *)hitActor->userData;
                                    if (userData) {
                                        result.target = userData->entity;
                                        result.position = PxVec3ToGlmVec3(hit.block.position);
                                        result.distance = hit.block.distance;
                                    }
                                }
                            }
                        } else if constexpr (std::is_same<T, ecs::PhysicsQuery::Overlap>()) {
                            if (entity.Has<ecs::TransformSnapshot>(lock)) {
                                auto &transform = entity.Get<ecs::TransformSnapshot>(lock).globalPose;

                                PxFilterData filterData;
                                filterData.word0 = (uint32_t)arg.filterGroup;

                                auto shapeTransform = transform * arg.shape.transform;
                                PxTransform pxTransform(GlmVec3ToPxVec3(shapeTransform.GetPosition()),
                                    GlmQuatToPxQuat(shapeTransform.GetRotation()));

                                PxOverlapHit touch;
                                PxOverlapBuffer hit;
                                hit.touches = &touch;
                                hit.maxNbTouches = 1;

                                manager.scene->overlap(manager.GeometryFromShape(arg.shape).any(),
                                    pxTransform,
                                    hit,
                                    PxQueryFilterData(filterData, PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC));

                                auto &result = arg.result.emplace();

                                physx::PxRigidActor *hitActor = touch.actor;
                                if (hitActor) {
                                    auto userData = (ActorUserData *)hitActor->userData;
                                    if (userData) {
                                        result = userData->entity;
                                    }
                                }
                            }
                        } else if constexpr (std::is_same<T, ecs::PhysicsQuery::Mass>()) {
                            auto target = arg.targetActor.Get(lock);
                            if (target) {
                                if (manager.actors.count(target) > 0) {
                                    auto &result = arg.result.emplace();

                                    const physx::PxRigidActor *actor = manager.actors[target];
                                    auto dynamic = actor->is<PxRigidDynamic>();
                                    if (dynamic) {
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
