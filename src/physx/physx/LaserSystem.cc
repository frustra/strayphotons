#include "LaserSystem.hh"

#include "core/Common.hh"
#include "core/Tracing.hh"
#include "ecs/Ecs.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/components/Physics.hh"
#include "physx/PhysxManager.hh"
#include "physx/PhysxUtils.hh"

#include <PxQueryReport.h>

namespace sp {
    using namespace physx;

    CVar<int> CVarLaserRecursion("x.LaserRecursion", 10, "maximum number of laser bounces");

    LaserSystem::LaserSystem(PhysxManager &manager) : manager(manager) {}

    struct OpticFilterCallback : PxQueryFilterCallback {
        OpticFilterCallback(ecs::Lock<ecs::Read<ecs::OpticalElement>> lock) : lock(lock) {}

        virtual PxQueryHitType::Enum preFilter(const PxFilterData &filterData,
            const PxShape *shape,
            const PxRigidActor *actor,
            PxHitFlags &queryFlags) {
            if (!actor) return PxQueryHitType::eNONE;
            auto userData = (ActorUserData *)actor->userData;
            if (!userData) return PxQueryHitType::eNONE;

            if (userData->entity.Has<ecs::OpticalElement>(lock)) {
                auto &optic = userData->entity.Get<ecs::OpticalElement>(lock);
                if (optic.type == ecs::OpticType::Gel) {
                    if (optic.tint == glm::vec3(1)) {
                        return PxQueryHitType::eNONE;
                    } else if (optic.tint * color != glm::vec3(0)) {
                        return PxQueryHitType::eTOUCH;
                    }
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

    void LaserSystem::Frame(ecs::Lock<ecs::Read<ecs::TransformSnapshot, ecs::LaserEmitter, ecs::OpticalElement>,
        ecs::Write<ecs::LaserLine, ecs::LaserSensor, ecs::SignalOutput>> lock) {
        ZoneScoped;

        for (auto &entity : lock.EntitiesWith<ecs::LaserSensor>()) {
            auto &sensor = entity.Get<ecs::LaserSensor>(lock);
            sensor.illuminance = glm::vec3(0);
        }
        for (auto &entity : lock.EntitiesWith<ecs::LaserEmitter>()) {
            if (!entity.Has<ecs::TransformSnapshot, ecs::LaserLine>(lock)) continue;

            auto &emitter = entity.Get<ecs::LaserEmitter>(lock);
            auto &lines = entity.Get<ecs::LaserLine>(lock);
            lines.on = emitter.on;
            if (!emitter.on) continue;

            auto &transform = entity.Get<ecs::TransformSnapshot>(lock);

            lines.intensity = emitter.intensity;
            lines.relative = false;

            if (!std::holds_alternative<ecs::LaserLine::Segments>(lines.line)) lines.line = ecs::LaserLine::Segments();
            auto &segments = std::get<ecs::LaserLine::Segments>(lines.line);
            segments.clear();
            color_t color = emitter.color;

            glm::vec3 rayStart = transform.GetPosition();
            glm::vec3 rayDir = transform.GetForward();

            std::array<physx::PxRaycastHit, 128> hitBuffer;

            PxRaycastBuffer hit;
            hit.touches = hitBuffer.data();
            hit.maxNbTouches = hitBuffer.size();
            PxFilterData filterData;
            filterData.word0 = (uint32_t)(ecs::PHYSICS_GROUP_WORLD | ecs::PHYSICS_GROUP_WORLD_OVERLAP |
                                          ecs::PHYSICS_GROUP_INTERACTIVE | ecs::PHYSICS_GROUP_PLAYER_LEFT_HAND |
                                          ecs::PHYSICS_GROUP_PLAYER_RIGHT_HAND);

            OpticFilterCallback filterCallback(lock);

            const float maxDistance = 1000.0f;
            int maxReflections = CVarLaserRecursion.Get();
            bool status = true;

            for (int reflectCount = 0; status && reflectCount <= maxReflections; reflectCount++) {
                filterCallback.color = color;
                status = manager.scene->raycast(GlmVec3ToPxVec3(rayStart),
                    GlmVec3ToPxVec3(rayDir),
                    maxDistance,
                    hit,
                    PxHitFlag::eNORMAL,
                    PxQueryFilterData(filterData,
                        PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC | PxQueryFlag::ePREFILTER),
                    &filterCallback);

                bool reflect = false;
                if (!status) {
                    auto &segment = segments.emplace_back();
                    segment.start = rayStart;
                    segment.end = rayStart + rayDir * maxDistance;
                    segment.color = color;
                    rayStart = segment.end;
                } else {
                    std::sort(hit.touches, hit.touches + hit.nbTouches, [](auto a, auto b) {
                        return a.distance < b.distance;
                    });

                    float startDistance = 0;
                    for (size_t i = 0; i < hit.nbTouches; i++) {
                        auto &touch = hit.touches[i];
                        if (!touch.actor) continue;
                        auto userData = (ActorUserData *)touch.actor->userData;
                        if (!userData) continue;
                        if (!userData->entity.Has<ecs::OpticalElement>(lock)) continue;
                        auto &optic = userData->entity.Get<ecs::OpticalElement>(lock);

                        auto &segment = segments.emplace_back();
                        segment.start = rayStart;
                        segment.end = rayStart + rayDir * (touch.distance - startDistance);
                        segment.color = color;
                        color *= optic.tint;
                        rayStart = segment.end;
                        startDistance = touch.distance;
                    }

                    auto &segment = segments.emplace_back();
                    segment.start = rayStart;
                    segment.end = rayStart + rayDir * (hit.block.distance - startDistance);
                    segment.color = color;
                    rayStart = segment.end;

                    physx::PxRigidActor *hitActor = hit.block.actor;
                    if (hitActor) {
                        auto userData = (ActorUserData *)hitActor->userData;
                        if (userData) {
                            auto hitEntity = userData->entity;
                            if (hitEntity.Has<ecs::OpticalElement>(lock)) {
                                auto &optic = hitEntity.Get<ecs::OpticalElement>(lock);
                                color *= optic.tint;
                                reflect = (optic.type == ecs::OpticType::Mirror);
                            }
                            if (hitEntity.Has<ecs::LaserSensor>(lock)) {
                                auto &sensor = hitEntity.Get<ecs::LaserSensor>(lock);
                                sensor.illuminance += glm::vec3(color * emitter.intensity);
                            }
                        }
                    }
                }

                if (reflect) {
                    rayDir = glm::normalize(glm::reflect(rayDir, PxVec3ToGlmVec3(hit.block.normal)));
                    rayStart += rayDir * 0.01f; // offset to prevent hitting the same object again
                } else {
                    break;
                }
            }
        }
        for (auto &entity : lock.EntitiesWith<ecs::LaserSensor>()) {
            if (!entity.Has<ecs::SignalOutput>(lock)) continue;
            auto &sensor = entity.Get<ecs::LaserSensor>(lock);
            auto &output = entity.Get<ecs::SignalOutput>(lock);
            output.SetSignal("light_value_r", sensor.illuminance.r);
            output.SetSignal("light_value_g", sensor.illuminance.g);
            output.SetSignal("light_value_b", sensor.illuminance.b);
            output.SetSignal("value", glm::all(glm::greaterThanEqual(sensor.illuminance, sensor.threshold)));
        }
    }
} // namespace sp
