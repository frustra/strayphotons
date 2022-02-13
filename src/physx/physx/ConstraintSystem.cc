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
    void ConstraintSystem::HandleForceLimitConstraint(PxRigidActor *actor,
        const ecs::Physics &physics,
        ecs::Transform transform,
        ecs::Transform targetTransform,
        glm::vec3 targetVelocity) {
        auto dynamic = actor->is<PxRigidDynamic>();
        if (!dynamic) return;
        auto centerOfMass = PxVec3ToGlmVec3(dynamic->getCMassLocalPose().p);

        auto currentRotate = transform.GetRotation();
        transform.Translate(currentRotate * centerOfMass);

        auto targetRotate = targetTransform.GetRotation();
        targetTransform.Translate(targetRotate * physics.constraintRotation * centerOfMass);

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

            auto targetRotationVelocity = deltaRotation;
            if (glm::length(maxVelocity) > glm::length(deltaTick)) {
                targetRotationVelocity = glm::normalize(targetRotationVelocity) * (maxVelocity - deltaTick);
            } else {
                targetRotationVelocity *= tickFrequency;
            }
            auto deltaVelocity = targetRotationVelocity - PxVec3ToGlmVec3(dynamic->getAngularVelocity());

            glm::vec3 force = worldInertia * (deltaVelocity * tickFrequency);
            float forceAbs = glm::length(force) + 0.00001f;
            auto forceClampRatio = std::min(CVarMaxConstraintTorque.Get(), forceAbs) / forceAbs;

            dynamic->addTorque(GlmVec3ToPxVec3(force) * forceClampRatio);
        }

        { // Apply Lateral Force
            auto maxAcceleration = CVarMaxLateralConstraintForce.Get() / dynamic->getMass();
            auto deltaTick = maxAcceleration * intervalSeconds;
            auto maxVelocity = std::sqrt(2 * maxAcceleration * glm::length(glm::vec2(deltaPos.x, deltaPos.z)));

            auto targetLateralVelocity = glm::vec3(deltaPos.x, 0, deltaPos.z);
            if (maxVelocity > deltaTick) {
                targetLateralVelocity = glm::normalize(targetLateralVelocity);
                targetLateralVelocity *= maxVelocity - deltaTick;
            } else {
                targetLateralVelocity *= tickFrequency;
            }
            targetLateralVelocity.x += targetVelocity.x;
            targetLateralVelocity.z += targetVelocity.z;
            auto deltaVelocity = targetLateralVelocity - PxVec3ToGlmVec3(dynamic->getLinearVelocity());
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

            float targetVerticalVelocity = 0.0f;
            if (maxVelocity > deltaTick) {
                targetVerticalVelocity = (maxVelocity - deltaTick) * (deltaPos.y > 0 ? 1 : -1);
            } else {
                targetVerticalVelocity = deltaPos.y * tickFrequency;
            }
            targetVerticalVelocity += targetVelocity.y;
            auto deltaVelocity = targetVerticalVelocity - dynamic->getLinearVelocity().y;

            float force = deltaVelocity * tickFrequency * dynamic->getMass();
            force -= CVarGravity.Get() * dynamic->getMass();
            float forceAbs = std::abs(force) + 0.00001f;
            auto forceClampRatio = std::min(CVarMaxVerticalConstraintForce.Get(), forceAbs) / forceAbs;
            dynamic->addForce(PxVec3(0, force * forceClampRatio, 0));
        }
    }

    void ConstraintSystem::BreakConstraints(
        ecs::Lock<ecs::Read<ecs::TransformSnapshot>, ecs::Write<ecs::Physics>> lock) {
        for (auto &entity : lock.EntitiesWith<ecs::Physics>()) {
            if (!entity.Has<ecs::Physics, ecs::TransformSnapshot>(lock)) continue;
            auto &physics = entity.Get<ecs::Physics>(lock);
            if (physics.constraintMaxDistance > 0.0f && physics.constraint.Has<ecs::TransformSnapshot>(lock)) {
                auto &transform = entity.Get<ecs::TransformSnapshot>(lock);
                auto &targetTransform = physics.constraint.Get<ecs::TransformSnapshot>(lock);

                auto deltaPos = targetTransform.GetPosition() - transform.GetPosition() +
                                targetTransform.GetRotation() * physics.constraintOffset;
                if (glm::length(deltaPos) > physics.constraintMaxDistance) physics.RemoveConstraint();
            }
        }
    }

    void ConstraintSystem::Frame(
        ecs::Lock<ecs::Read<ecs::TransformTree, ecs::CharacterController, ecs::Physics>> lock) {
        for (auto &entity : lock.EntitiesWith<ecs::Physics>()) {
            if (!entity.Has<ecs::Physics, ecs::TransformTree>(lock)) continue;
            if (manager.actors.count(entity) == 0) continue;

            auto &physics = entity.Get<ecs::Physics>(lock);
            auto const &actor = manager.actors[entity];

            ecs::PhysicsJointType currentJointType = ecs::PhysicsJointType::Count;
            if (manager.joints.count(entity) > 0) {
                auto &joint = manager.joints[entity];
                if (joint->is<physx::PxFixedJoint>()) {
                    currentJointType = ecs::PhysicsJointType::Fixed;
                } else if (joint->is<physx::PxDistanceJoint>()) {
                    currentJointType = ecs::PhysicsJointType::Distance;
                } else if (joint->is<physx::PxSphericalJoint>()) {
                    currentJointType = ecs::PhysicsJointType::Spherical;
                } else if (joint->is<physx::PxRevoluteJoint>()) {
                    currentJointType = ecs::PhysicsJointType::Hinge;
                } else if (joint->is<physx::PxPrismaticJoint>()) {
                    currentJointType = ecs::PhysicsJointType::Slider;
                } else {
                    Abortf("Unknown PxJoint type: %u", joint->getConcreteType());
                }
                if (!physics.jointTarget || physics.jointType != currentJointType) {
                    joint->release();
                    manager.joints.erase(entity);

                    auto dynamic = actor->is<PxRigidDynamic>();
                    if (dynamic) dynamic->wakeUp();
                }
            }

            if (physics.jointTarget) {
                physx::PxRigidActor *jointActor = nullptr;
                PxTransform localTransform(GlmVec3ToPxVec3(physics.jointLocalOffset),
                    GlmQuatToPxQuat(physics.jointLocalOrient));
                PxTransform remoteTransform(PxIdentity);

                if (manager.actors.count(physics.jointTarget) > 0) {
                    jointActor = manager.actors[physics.jointTarget];
                    remoteTransform.p = GlmVec3ToPxVec3(physics.jointRemoteOffset);
                    remoteTransform.q = GlmQuatToPxQuat(physics.jointRemoteOrient);
                }
                if (!jointActor && physics.jointTarget.Has<ecs::TransformTree>(lock)) {
                    auto transform = physics.jointTarget.Get<ecs::TransformTree>(lock).GetGlobalTransform(lock);
                    auto rotate = transform.GetRotation();
                    remoteTransform.p = GlmVec3ToPxVec3(transform.GetPosition() + rotate * physics.jointRemoteOffset);
                    remoteTransform.q = GlmQuatToPxQuat(rotate * physics.jointRemoteOrient);
                }

                auto &joint = manager.joints[entity];
                if (joint == nullptr) {
                    if (physics.jointType == ecs::PhysicsJointType::Fixed) {
                        joint =
                            PxFixedJointCreate(*manager.pxPhysics, actor, localTransform, jointActor, remoteTransform);
                    } else if (physics.jointType == ecs::PhysicsJointType::Distance) {
                        auto distanceJoint = PxDistanceJointCreate(*manager.pxPhysics,
                            actor,
                            localTransform,
                            jointActor,
                            remoteTransform);
                        distanceJoint->setMinDistance(physics.jointRange.x);
                        if (physics.jointRange.y > physics.jointRange.x) {
                            distanceJoint->setMaxDistance(physics.jointRange.y);
                            distanceJoint->setDistanceJointFlag(PxDistanceJointFlag::eMAX_DISTANCE_ENABLED, true);
                        }
                        joint = distanceJoint;
                    } else if (physics.jointType == ecs::PhysicsJointType::Spherical) {
                        auto sphericalJoint = PxSphericalJointCreate(*manager.pxPhysics,
                            actor,
                            localTransform,
                            jointActor,
                            remoteTransform);
                        if (physics.jointRange.x != 0.0f || physics.jointRange.y != 0.0f) {
                            sphericalJoint->setLimitCone(PxJointLimitCone(glm::radians(physics.jointRange.x),
                                glm::radians(physics.jointRange.y)));
                            sphericalJoint->setSphericalJointFlag(PxSphericalJointFlag::eLIMIT_ENABLED, true);
                            sphericalJoint->setConstraintFlag(PxConstraintFlag::eENABLE_EXTENDED_LIMITS, true);
                        }
                        joint = sphericalJoint;
                    } else if (physics.jointType == ecs::PhysicsJointType::Hinge) {
                        auto revoluteJoint = PxRevoluteJointCreate(*manager.pxPhysics,
                            actor,
                            localTransform,
                            jointActor,
                            remoteTransform);
                        if (physics.jointRange.x != 0.0f || physics.jointRange.y != 0.0f) {
                            revoluteJoint->setLimit(PxJointAngularLimitPair(glm::radians(physics.jointRange.x),
                                glm::radians(physics.jointRange.y)));
                            revoluteJoint->setRevoluteJointFlag(PxRevoluteJointFlag::eLIMIT_ENABLED, true);
                            revoluteJoint->setConstraintFlag(PxConstraintFlag::eENABLE_EXTENDED_LIMITS, true);
                        }
                        joint = revoluteJoint;
                    } else if (physics.jointType == ecs::PhysicsJointType::Slider) {
                        auto prismaticJoint = PxPrismaticJointCreate(*manager.pxPhysics,
                            actor,
                            localTransform,
                            jointActor,
                            remoteTransform);
                        if (physics.jointRange.x != 0.0f || physics.jointRange.y != 0.0f) {
                            prismaticJoint->setLimit(PxJointLinearLimitPair(manager.pxPhysics->getTolerancesScale(),
                                physics.jointRange.x,
                                physics.jointRange.y));
                            prismaticJoint->setPrismaticJointFlag(PxPrismaticJointFlag::eLIMIT_ENABLED, true);
                        }
                        joint = prismaticJoint;
                    } else {
                        Abortf("Unsupported PhysX joint type: %u", physics.jointType);
                    }
                } else {
                    joint->setActors(actor, jointActor);
                    joint->setLocalPose(PxJointActorIndex::eACTOR0, localTransform);
                    joint->setLocalPose(PxJointActorIndex::eACTOR1, remoteTransform);
                }
                auto dynamic = actor->is<PxRigidDynamic>();
                if (dynamic) dynamic->wakeUp();
            }

            if (physics.constraint.Has<ecs::TransformTree>(lock)) {
                auto transform = entity.Get<ecs::TransformTree>(lock).GetGlobalTransform(lock);
                auto &targetTransform = physics.constraint.Get<ecs::TransformTree>(lock);
                glm::vec3 targetVelocity(0);

                // Try and determine the velocity of the constraint target entity
                auto e = physics.constraint;
                while (e.Has<ecs::TransformTree>(lock)) {
                    if (manager.actors.count(e) > 0) {
                        auto userData = (ActorUserData *)manager.actors[e]->userData;
                        if (userData) targetVelocity = userData->velocity;
                        break;
                    } else if (e.Has<ecs::CharacterController>(lock)) {
                        auto &targetController = e.Get<ecs::CharacterController>(lock);
                        if (targetController.pxController) {
                            auto userData = (CharacterControllerUserData *)targetController.pxController->getUserData();
                            if (userData) targetVelocity = userData->actorData.velocity;
                            break;
                        }
                    }
                    e = e.Get<ecs::TransformTree>(lock).parent;
                }

                HandleForceLimitConstraint(actor,
                    physics,
                    transform,
                    targetTransform.GetGlobalTransform(lock),
                    targetVelocity);
            }

            if (physics.constantForce != glm::vec3()) {
                auto rotation = entity.Get<ecs::TransformTree>(lock).GetGlobalRotation(lock);
                auto dynamic = actor->is<PxRigidDynamic>();
                if (dynamic) dynamic->addForce(GlmVec3ToPxVec3(rotation * physics.constantForce));
            }
        }
    }
} // namespace sp
