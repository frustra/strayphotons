#include "ConstraintSystem.hh"

#include "console/CVar.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/Ecs.hh"
#include "ecs/EcsImpl.hh"
#include "physx/PhysxManager.hh"
#include "physx/PhysxUtils.hh"

#include <glm/glm.hpp>

namespace sp {
    using namespace physx;

    // clang-format off
    static CVar<float> CVarMaxVerticalConstraintForce("x.MaxVerticalConstraintForce", 20.0f, "The maximum linear lifting force for constraints");
    static CVar<float> CVarMaxLateralConstraintForce("x.MaxLateralConstraintForce", 20.0f, "The maximum lateral force for constraints");
    static CVar<float> CVarMaxConstraintTorque("x.MaxConstraintTorque", 10.0f, "The maximum torque force for constraints");
    // clang-format on

    ConstraintSystem::ConstraintSystem(PhysxManager &manager) : manager(manager) {}

    /**
     * This constraint system operates by applying forces to an object's center of mass up to a specified maximum.
     *
     * Constrained actor velocities are capped at a calculated maximum in order for them to be able to stop on target
     * without exceeding force limits. Additionally, force along the vertical axis will take gravity into account to
     * keep objects at their target position and orientation.
     *
     * If a constraint's target distance exceeds its maximum, the constraint will break and be removed.
     */
    void ConstraintSystem::Frame(ecs::Lock<ecs::Read<ecs::Transform>, ecs::Write<ecs::Physics>> lock) {
        for (auto &entity : lock.EntitiesWith<ecs::Physics>()) {
            if (!entity.Has<ecs::Physics, ecs::Transform>(lock)) continue;
            auto &physics = entity.Get<ecs::Physics>(lock);
            if (!physics.constraint) continue;
            Assertf(physics.constraint.Has<ecs::Transform>(lock), "Physics entity has invalid constraint target");
            if (!physics.actor) continue;
            auto dynamic = physics.actor->is<PxRigidDynamic>();
            if (dynamic == nullptr) return;

            auto transform = entity.Get<ecs::Transform>(lock).GetGlobalTransform(lock);
            auto currentRotate = transform.GetRotation();
            transform.Translate(currentRotate * physics.centerOfMass);

            auto targetTransform = physics.constraint.Get<ecs::Transform>(lock).GetGlobalTransform(lock);
            auto targetRotate = targetTransform.GetRotation();
            targetTransform.Translate(targetRotate * physics.constraintRotation * physics.centerOfMass);

            auto deltaPos = targetTransform.GetPosition() - transform.GetPosition() +
                            targetRotate * physics.constraintOffset;
            auto deltaRotate = targetRotate * physics.constraintRotation * glm::inverse(currentRotate);

            float intervalSeconds = manager.interval.count() / 1e9;
            float tickFrequency = 1e9 / manager.interval.count();

            { // Apply Torque
                auto deltaRotation = glm::eulerAngles(deltaRotate);

                auto massInertia = PxVec3ToGlmVec3(dynamic->getMassSpaceInertiaTensor());
                auto invMassInertia = PxVec3ToGlmVec3(dynamic->getMassSpaceInvInertiaTensor());
                glm::mat3 worldInertia = InertiaTensorMassToWorld(massInertia, currentRotate);
                glm::mat3 invWorldInertia = InertiaTensorMassToWorld(invMassInertia, currentRotate);

                auto maxAcceleration = invWorldInertia * glm::vec3(CVarMaxConstraintTorque.Get());
                auto deltaTick = maxAcceleration * intervalSeconds;
                auto maxVelocity = glm::vec3(std::sqrt(2 * maxAcceleration.x * std::abs(deltaRotation.x)),
                    std::sqrt(2 * maxAcceleration.y * std::abs(deltaRotation.y)),
                    std::sqrt(2 * maxAcceleration.z * std::abs(deltaRotation.z)));

                auto targetVelocity = deltaRotation;
                if (glm::length(maxVelocity) > glm::length(deltaTick)) {
                    targetVelocity = glm::normalize(targetVelocity) * (maxVelocity - deltaTick);
                } else {
                    targetVelocity *= tickFrequency;
                }
                auto deltaVelocity = targetVelocity - PxVec3ToGlmVec3(dynamic->getAngularVelocity());

                glm::vec3 force = worldInertia * (deltaVelocity * tickFrequency);
                float forceAbs = glm::length(force) + 0.00001f;
                auto forceClampRatio = std::min(CVarMaxConstraintTorque.Get(), forceAbs) / forceAbs;

                dynamic->addTorque(GlmVec3ToPxVec3(force) * forceClampRatio);
            }

            { // Apply Lateral Force
                auto maxAcceleration = CVarMaxLateralConstraintForce.Get() / dynamic->getMass();
                auto deltaTick = maxAcceleration * intervalSeconds;
                auto maxVelocity = std::sqrt(2 * maxAcceleration * glm::length(glm::vec2(deltaPos.x, deltaPos.z)));

                auto targetVelocity = glm::vec3(deltaPos.x, 0, deltaPos.z);
                if (maxVelocity > deltaTick) {
                    targetVelocity = glm::normalize(targetVelocity);
                    targetVelocity *= maxVelocity - deltaTick;
                } else {
                    targetVelocity *= tickFrequency;
                }
                auto deltaVelocity = targetVelocity - PxVec3ToGlmVec3(dynamic->getLinearVelocity());
                deltaVelocity.y = 0;

                glm::vec3 force = deltaVelocity * tickFrequency * dynamic->getMass();
                float forceAbs = glm::length(force) + 0.00001f;
                auto forceClampRatio = std::min(CVarMaxLateralConstraintForce.Get(), forceAbs) / forceAbs;
                dynamic->addForce(GlmVec3ToPxVec3(force) * forceClampRatio);
            }

            { // Apply Vertical Force
                auto maxAcceleration = CVarMaxVerticalConstraintForce.Get() / dynamic->getMass();
                if (deltaPos.y > 0) {
                    maxAcceleration -= CVarGravity.Get();
                } else {
                    maxAcceleration += CVarGravity.Get();
                }
                auto deltaTick = maxAcceleration * intervalSeconds;
                auto maxVelocity = std::sqrt(2 * std::max(0.0f, maxAcceleration) * std::abs(deltaPos.y));

                float targetVelocity = 0.0f;
                if (maxVelocity > deltaTick) {
                    targetVelocity = (maxVelocity - deltaTick) * (deltaPos.y > 0 ? 1 : -1);
                } else {
                    targetVelocity = deltaPos.y * tickFrequency;
                }
                auto deltaVelocity = targetVelocity - dynamic->getLinearVelocity().y;

                float force = deltaVelocity * tickFrequency * dynamic->getMass();
                force -= CVarGravity.Get() * dynamic->getMass();
                float forceAbs = std::abs(force) + 0.00001f;
                auto forceClampRatio = std::min(CVarMaxVerticalConstraintForce.Get(), forceAbs) / forceAbs;
                dynamic->addForce(PxVec3(0, force * forceClampRatio, 0));
            }

            if (physics.constraintMaxDistance > 0.0f && glm::length(deltaPos) > physics.constraintMaxDistance) {
                physics.RemoveConstraint();
            }
        }
    }
} // namespace sp
