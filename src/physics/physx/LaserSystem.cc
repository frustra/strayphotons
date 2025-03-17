/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "LaserSystem.hh"

#include "common/Common.hh"
#include "common/Tracing.hh"
#include "ecs/EcsImpl.hh"
#include "physx/PhysxManager.hh"
#include "physx/PhysxUtils.hh"

#include <PxQueryReport.h>

namespace sp {
    using namespace physx;

    CVar<int> CVarLaserRecursion("x.LaserRecursion", 10, "maximum number of laser bounces");
    CVar<float> CVarLaserBounceOffset("x.LaserBounceOffset", 0.001f, "Distance to offset laser bounces");

    LaserSystem::LaserSystem(PhysxManager &manager) : manager(manager) {}

    struct OpticFilterCallback : PxQueryFilterCallback {
        OpticFilterCallback(ecs::Lock<ecs::Read<ecs::OpticalElement>> lock) : lock(lock) {}

        virtual PxQueryHitType::Enum preFilter(const PxFilterData &filterData,
            const PxShape *shape,
            const PxRigidActor *actor,
            PxHitFlags &queryFlags) {
            if (!actor) return PxQueryHitType::eNONE;
            auto userData = (ShapeUserData *)shape->userData;
            if (!userData) return PxQueryHitType::eNONE;

            if (userData->owner.Has<ecs::OpticalElement>(lock)) {
                auto &optic = userData->owner.Get<ecs::OpticalElement>(lock);
                if (optic.passTint == glm::vec3(1)) {
                    if (color * optic.reflectTint == glm::vec3(0)) {
                        return PxQueryHitType::eNONE;
                    } else {
                        return PxQueryHitType::eTOUCH;
                    }
                } else if (color * optic.passTint == glm::vec3(0)) {
                    if (optic.singleDirection) {
                        return PxQueryHitType::eTOUCH;
                    } else {
                        return PxQueryHitType::eBLOCK;
                    }
                } else {
                    return PxQueryHitType::eTOUCH;
                }
            }
            return PxQueryHitType::eBLOCK;
        }

        virtual PxQueryHitType::Enum postFilter(const PxFilterData &filterData, const PxQueryHit &hit) {
            return PxQueryHitType::eNONE;
        }

        ecs::Lock<ecs::Read<ecs::OpticalElement>> lock;
        color_t color;
    };

    void LaserSystem::Frame(ecs::Lock<ecs::ReadSignalsLock,
        ecs::Read<ecs::TransformSnapshot, ecs::LaserEmitter, ecs::OpticalElement>,
        ecs::Write<ecs::LaserLine, ecs::LaserSensor, ecs::Signals>> lock) {
        ZoneScoped;

        struct LaserStart {
            glm::vec3 rayStart, rayDir;
            color_t color;
            int depth = 0;
        };

        static std::vector<LaserStart> emitterQueue;

        for (auto &entity : lock.EntitiesWith<ecs::LaserSensor>()) {
            auto &sensor = entity.Get<ecs::LaserSensor>(lock);
            sensor.illuminance = {};
        }
        for (auto &entity : lock.EntitiesWith<ecs::LaserEmitter>()) {
            if (!entity.Has<ecs::Name, ecs::TransformSnapshot, ecs::LaserLine>(lock)) continue;

            auto &name = entity.Get<ecs::Name>(lock);
            auto &emitter = entity.Get<ecs::LaserEmitter>(lock);
            auto &lines = entity.Get<ecs::LaserLine>(lock);
            lines.on = emitter.on;
            if (!emitter.on) continue;

            auto &transform = entity.Get<ecs::TransformSnapshot>(lock).globalPose;

            lines.intensity = emitter.intensity;
            lines.relative = false;

            if (!std::holds_alternative<ecs::LaserLine::Segments>(lines.line)) lines.line = ecs::LaserLine::Segments();
            auto &segments = std::get<ecs::LaserLine::Segments>(lines.line);
            segments.clear();

            std::array<ecs::SignalExpression, 3> signalColor = {
                ecs::SignalExpression{ecs::SignalRef(entity, "laser_color_r")},
                {ecs::SignalRef(entity, "laser_color_g")},
                {ecs::SignalRef(entity, "laser_color_b")},
            };
            signalColor[0] = signalColor[0] + (name.String() + "#laser_emitter.color.r");
            signalColor[1] = signalColor[1] + (name.String() + "#laser_emitter.color.g");
            signalColor[2] = signalColor[2] + (name.String() + "#laser_emitter.color.b");
            signalColor[0] = signalColor[0] * (name.String() + "#laser_emitter.intensity");
            signalColor[1] = signalColor[1] * (name.String() + "#laser_emitter.intensity");
            signalColor[2] = signalColor[2] * (name.String() + "#laser_emitter.intensity");

            std::array<physx::PxRaycastHit, 128> hitBuffer;

            PxRaycastBuffer hit;
            hit.touches = hitBuffer.data();
            hit.maxNbTouches = hitBuffer.size();
            PxFilterData filterData;
            filterData.word0 = (uint32_t)(ecs::PHYSICS_GROUP_WORLD | ecs::PHYSICS_GROUP_INTERACTIVE |
                                          ecs::PHYSICS_GROUP_HELD_OBJECT | ecs::PHYSICS_GROUP_PLAYER_LEFT_HAND |
                                          ecs::PHYSICS_GROUP_PLAYER_RIGHT_HAND);

            OpticFilterCallback filterCallback(lock);

            const float maxDistance = 1000.0f;
            int maxReflections = CVarLaserRecursion.Get();
            float bounceOffset = CVarLaserBounceOffset.Get();
            bool status = true;

            emitterQueue.clear();
            emitterQueue.emplace_back(LaserStart{
                transform.GetPosition() + transform.GetForward() * emitter.startDistance * transform.GetScale(),
                transform.GetForward(),
                glm::vec3(1),
            });

            while (!emitterQueue.empty()) {
                auto laserStart = emitterQueue.back();
                emitterQueue.pop_back();
                laserStart.depth++;
                if (laserStart.depth > maxReflections) continue;

                filterCallback.color = laserStart.color;
                status = manager.scene->raycast(GlmVec3ToPxVec3(laserStart.rayStart),
                    GlmVec3ToPxVec3(laserStart.rayDir),
                    maxDistance,
                    hit,
                    PxHitFlag::eNORMAL,
                    PxQueryFilterData(filterData,
                        PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC | PxQueryFlag::ePREFILTER),
                    &filterCallback);

                if (!status) {
                    auto &segment = segments.emplace_back();
                    segment.start = laserStart.rayStart;
                    segment.end = laserStart.rayStart + laserStart.rayDir * maxDistance;
                    segment.color[0] = signalColor[0] * laserStart.color[0];
                    segment.color[1] = signalColor[1] * laserStart.color[1];
                    segment.color[2] = signalColor[2] * laserStart.color[2];
                } else {
                    std::sort(hit.touches, hit.touches + hit.nbTouches, [](auto a, auto b) {
                        return a.distance < b.distance;
                    });

                    float startDistance = 0;
                    for (size_t i = 0; i < hit.nbTouches; i++) {
                        auto &touch = hit.touches[i];
                        if (!touch.actor) continue;
                        auto userData = (ShapeUserData *)touch.shape->userData;
                        if (!userData) continue;
                        if (!userData->owner.Has<ecs::OpticalElement, ecs::TransformSnapshot>(lock)) continue;
                        auto &optic = userData->owner.Get<ecs::OpticalElement>(lock);
                        auto &opticTransform = userData->owner.Get<ecs::TransformSnapshot>(lock).globalPose;
                        if (optic.singleDirection && glm::dot(opticTransform.GetForward(), laserStart.rayDir) > 0) {
                            continue;
                        }

                        auto segmentEnd = laserStart.rayStart + laserStart.rayDir * (touch.distance - startDistance);

                        if (laserStart.color * optic.reflectTint != glm::vec3(0)) {
                            auto &reflectionStart = emitterQueue.emplace_back();
                            reflectionStart.rayDir = glm::normalize(
                                glm::reflect(laserStart.rayDir, PxVec3ToGlmVec3(touch.normal)));
                            // offset to prevent hitting the same object again
                            reflectionStart.rayStart = segmentEnd + reflectionStart.rayDir * bounceOffset;
                            reflectionStart.color = laserStart.color * optic.reflectTint;
                            reflectionStart.depth = laserStart.depth;
                        }
                        if (laserStart.color * optic.passTint != glm::vec3(0)) {
                            auto &segment = segments.emplace_back();
                            segment.start = laserStart.rayStart;
                            segment.end = segmentEnd;
                            segment.color[0] = signalColor[0] * laserStart.color[0];
                            segment.color[1] = signalColor[1] * laserStart.color[1];
                            segment.color[2] = signalColor[2] * laserStart.color[2];

                            laserStart.color *= optic.passTint;
                            laserStart.rayStart = segmentEnd;
                            startDistance = touch.distance;
                        } else {
                            hit.hasBlock = true;
                            hit.block = touch;
                            break;
                        }
                    }

                    auto &segment = segments.emplace_back();
                    segment.start = laserStart.rayStart;
                    segment.end = laserStart.rayStart +
                                  laserStart.rayDir *
                                      ((hit.hasBlock ? hit.block.distance : maxDistance) - startDistance);
                    segment.color[0] = signalColor[0] * laserStart.color[0];
                    segment.color[1] = signalColor[1] * laserStart.color[1];
                    segment.color[2] = signalColor[2] * laserStart.color[2];

                    physx::PxShape *hitShape = hit.block.shape;
                    if (hitShape) {
                        auto userData = (ShapeUserData *)hitShape->userData;
                        if (userData) {
                            auto hitEntity = userData->owner;
                            if (hitEntity.Has<ecs::LaserSensor>(lock)) {
                                auto &sensor = hitEntity.Get<ecs::LaserSensor>(lock);
                                sensor.illuminance[0] = signalColor[0] * laserStart.color[0] + sensor.illuminance[0];
                                sensor.illuminance[1] = signalColor[1] * laserStart.color[1] + sensor.illuminance[1];
                                sensor.illuminance[2] = signalColor[2] * laserStart.color[2] + sensor.illuminance[2];
                            }
                            if (hitEntity.Has<ecs::OpticalElement>(lock)) {
                                auto &optic = hitEntity.Get<ecs::OpticalElement>(lock);
                                if (laserStart.color * optic.reflectTint != glm::vec3(0)) {
                                    laserStart.color *= optic.reflectTint;
                                    laserStart.rayDir = glm::normalize(
                                        glm::reflect(laserStart.rayDir, PxVec3ToGlmVec3(hit.block.normal)));
                                    // offset to prevent hitting the same object again
                                    laserStart.rayStart = segment.end + laserStart.rayDir * bounceOffset;
                                    emitterQueue.emplace_back(laserStart);
                                }
                            }
                        }
                    }
                }
            }
        }
        for (auto &entity : lock.EntitiesWith<ecs::LaserSensor>()) {
            auto &sensor = entity.Get<ecs::LaserSensor>(lock);
            ecs::SignalRef(entity, "light_value_r").SetBinding(lock, sensor.illuminance[0]);
            ecs::SignalRef(entity, "light_value_g").SetBinding(lock, sensor.illuminance[1]);
            ecs::SignalRef(entity, "light_value_b").SetBinding(lock, sensor.illuminance[2]);
            ecs::SignalRef(entity, "value")
                .SetBinding(lock,
                    "( " + sensor.illuminance[0].expr + " >= " + std::to_string(sensor.threshold.r) + ") && ( " +
                        sensor.illuminance[1].expr + " >= " + std::to_string(sensor.threshold.g) + ") && ( " +
                        sensor.illuminance[2].expr + " >= " + std::to_string(sensor.threshold.b) + ")");
        }
    }
} // namespace sp
