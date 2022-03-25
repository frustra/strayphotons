#include "LaserSystem.hh"

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

    void LaserSystem::Frame(ecs::Lock<ecs::Read<ecs::TransformSnapshot, ecs::LaserEmitter, ecs::OpticalElement>,
        ecs::Write<ecs::LaserLine, ecs::LaserSensor, ecs::SignalOutput>> lock) {
        ZoneScoped;
        for (auto &entity : lock.EntitiesWith<ecs::LaserSensor>()) {
            auto &sensor = entity.Get<ecs::LaserSensor>(lock);
            sensor.illuminance = glm::vec3(0);
        }
        for (auto &entity : lock.EntitiesWith<ecs::LaserEmitter>()) {
            if (!entity.Has<ecs::TransformSnapshot>(lock)) continue;

            auto &emitter = entity.Get<ecs::LaserEmitter>(lock);
            if (!emitter.on) continue;

            auto &transform = entity.Get<ecs::TransformSnapshot>(lock);

            auto &lines = entity.Get<ecs::LaserLine>(lock);
            lines.on = true;
            lines.intensity = emitter.intensity;
            lines.relative = false;

            auto &line = std::get<ecs::LaserLine::Line>(lines.line);
            line.color = emitter.color;
            line.points.clear();

            glm::vec3 rayStart = transform.GetPosition();
            glm::vec3 rayDir = transform.GetForward();
            line.points.push_back(rayStart);

            PxRaycastBuffer hit;
            PxFilterData filterData;
            filterData.word0 = (uint32_t)(ecs::PHYSICS_GROUP_WORLD | ecs::PHYSICS_GROUP_PLAYER_HANDS);

            const float maxDistance = 1000.0f;
            int maxReflections = CVarLaserRecursion.Get();
            bool status = true;

            for (int reflectCount = 0; status && reflectCount <= maxReflections; reflectCount++) {
                status = manager.scene->raycast(GlmVec3ToPxVec3(rayStart),
                    GlmVec3ToPxVec3(rayDir),
                    maxDistance,
                    hit,
                    PxHitFlag::eNORMAL,
                    PxQueryFilterData(filterData, PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC));

                glm::vec3 hitPos;
                bool reflect = false;
                if (!status) {
                    hitPos = rayStart + rayDir * maxDistance;
                } else {
                    hitPos = rayStart + rayDir * hit.block.distance;

                    physx::PxRigidActor *hitActor = hit.block.actor;
                    if (hitActor) {
                        auto userData = (ActorUserData *)hitActor->userData;
                        if (userData) {
                            auto hitEntity = userData->entity;
                            if (hitEntity.Has<ecs::OpticalElement>(lock)) {
                                auto &optic = hitEntity.Get<ecs::OpticalElement>(lock);
                                reflect = (optic.type == ecs::OpticType::Mirror);
                            }
                            if (hitEntity.Has<ecs::LaserSensor>(lock)) {
                                auto &sensor = hitEntity.Get<ecs::LaserSensor>(lock);
                                sensor.illuminance += emitter.color * emitter.intensity;
                            }
                        }
                    }
                }

                line.points.push_back(hitPos);

                if (reflect) {
                    rayDir = glm::normalize(glm::reflect(rayDir, PxVec3ToGlmVec3(hit.block.normal)));
                    rayStart = hitPos + rayDir * 0.01f; // offset to prevent hitting the same object again
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
