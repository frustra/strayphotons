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
    CVar<float> CVarLaserBounceOffset("x.LaserBounceOffset", 0.001f, "Distance to offset laser bounces");

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
        ecs::Write<ecs::LaserLine, ecs::LaserSensor, ecs::SignalOutput>> lock) {
        ZoneScoped;

        struct LaserStart {
            glm::vec3 rayStart, rayDir;
            color_t color;
            int depth = 0;
        };

        static std::vector<LaserStart> emitterQueue;

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

            color_t signalColor = glm::vec3{
                ecs::SignalBindings::GetSignal(lock, entity, "laser_color_r"),
                ecs::SignalBindings::GetSignal(lock, entity, "laser_color_g"),
                ecs::SignalBindings::GetSignal(lock, entity, "laser_color_b"),
            };

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
            float bounceOffset = CVarLaserBounceOffset.Get();
            bool status = true;

            emitterQueue.clear();
            emitterQueue.emplace_back(LaserStart{
                transform.GetPosition() + transform.GetForward() * emitter.startDistance * transform.GetScale(),
                transform.GetForward(),
                emitter.color + signalColor,
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
                    segment.color = laserStart.color;
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
                        if (!userData->entity.Has<ecs::OpticalElement, ecs::TransformSnapshot>(lock)) continue;
                        auto &optic = userData->entity.Get<ecs::OpticalElement>(lock);
                        auto &opticTransform = userData->entity.Get<ecs::TransformSnapshot>(lock);
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
                            segment.color = laserStart.color;

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
                    segment.color = laserStart.color;

                    physx::PxRigidActor *hitActor = hit.block.actor;
                    if (hitActor) {
                        auto userData = (ActorUserData *)hitActor->userData;
                        if (userData) {
                            auto hitEntity = userData->entity;
                            if (hitEntity.Has<ecs::LaserSensor>(lock)) {
                                auto &sensor = hitEntity.Get<ecs::LaserSensor>(lock);
                                sensor.illuminance += glm::vec3(laserStart.color * emitter.intensity);
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
