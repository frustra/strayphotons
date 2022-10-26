#include "ConstraintSystem.hh"

#include "console/CVar.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/Ecs.hh"
#include "ecs/EcsImpl.hh"
#include "input/BindingNames.hh"
#include "physx/ForceConstraint.hh"
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
     * Forces are applied via 1D springs in PhysX's built-in constraint solver.
     *
     * Constrained actor velocities are capped at a calculated maximum in order for them to be able to stop on target
     * without exceeding force limits. Additionally, a gravity-oriented lift force will be applied separetely to make
     * trajectories more stable at the force limit.
     */
    bool ConstraintSystem::UpdateForceConstraint(PxRigidActor *actor,
        JointState *joint,
        ecs::Transform transform,
        ecs::Transform targetTransform,
        glm::vec3 targetVelocity,
        glm::vec3 gravity) {
        if (!actor || !joint || !joint->forceConstraint) return false;
        auto dynamic = actor->is<PxRigidDynamic>();
        if (!dynamic) return false;
        auto centerOfMass = PxVec3ToGlmVec3(dynamic->getCMassLocalPose().p);

        auto currentRotate = transform.GetRotation();
        transform.Translate(currentRotate * centerOfMass);
        auto targetRotate = targetTransform.GetRotation();
        targetTransform.Translate(targetRotate * centerOfMass);
        auto deltaPos = targetTransform.GetPosition() - transform.GetPosition();

        float intervalSeconds = manager.interval.count() / 1e9;
        float tickFrequency = 1e9 / manager.interval.count();

        float maxForce = joint->ecsJoint.limit.x;
        float maxTorque = joint->ecsJoint.limit.y;

        float magneticRadiusScale = 1.0f;
        if (joint->ecsJoint.type == ecs::PhysicsJointType::Magnetic) {
            auto distance = glm::length(deltaPos);
            if (distance > joint->ecsJoint.magnetRadius || joint->ecsJoint.magnetRadius <= 0.0f) {
                magneticRadiusScale = 0.0f;
            } else {
                magneticRadiusScale = 1.0f - (distance / joint->ecsJoint.magnetRadius);
            }
        }

        bool wakeUp = false;

        // Update Torque
        if (maxTorque > 0) {
            if (glm::dot(currentRotate, targetRotate) < 0.0f) { // minimum distance quaternion
                targetRotate = -targetRotate;
            }
            auto deltaRotation = glm::eulerAngles(glm::inverse(currentRotate) * targetRotate);
            auto maxAcceleration = PxVec3ToGlmVec3(dynamic->getMassSpaceInvInertiaTensor()) * maxTorque;
            auto deltaTick = maxAcceleration * intervalSeconds;
            auto maxVelocity = glm::vec3(std::sqrt(2 * maxAcceleration.x * std::abs(deltaRotation.x)),
                std::sqrt(2 * maxAcceleration.y * std::abs(deltaRotation.y)),
                std::sqrt(2 * maxAcceleration.z * std::abs(deltaRotation.z)));

            glm::vec3 targetAngularVelocity(0);
            for (int i = 0; i < 3; i++) {
                if (maxVelocity[i] > deltaTick[i]) {
                    targetAngularVelocity[i] = glm::sign(deltaRotation[i]) * (maxVelocity[i] - deltaTick[i]);
                } else {
                    targetAngularVelocity[i] = deltaRotation[i] * tickFrequency;
                }
            }

            auto currentAngularVelocity = glm::inverse(currentRotate) * PxVec3ToGlmVec3(dynamic->getAngularVelocity());
            auto deltaVelocity = targetAngularVelocity - currentAngularVelocity;

            glm::vec3 accel = deltaVelocity * tickFrequency;
            glm::vec3 accelAbs = glm::abs(accel) + 0.00001f;
            auto clampRatio = glm::min(maxAcceleration, accelAbs) / accelAbs;
            wakeUp |= joint->forceConstraint->setAngularAccel(accel * clampRatio * magneticRadiusScale);
        } else {
            wakeUp |= joint->forceConstraint->setAngularAccel(glm::vec3(0));
        }

        // Update Linear Force
        if (maxForce > 0) {
            auto maxAcceleration = maxForce / dynamic->getMass();
            auto deltaTick = maxAcceleration * intervalSeconds;
            auto maxVelocity = std::sqrt(2 * maxAcceleration * glm::length(deltaPos));

            auto targetLinearVelocity = deltaPos * tickFrequency;
            if (maxVelocity > deltaTick) {
                targetLinearVelocity = glm::normalize(deltaPos) * (maxVelocity - deltaTick);
            }
            targetLinearVelocity += targetVelocity;
            auto deltaVelocity = targetLinearVelocity - PxVec3ToGlmVec3(dynamic->getLinearVelocity());

            glm::vec3 accel = deltaVelocity * tickFrequency;
            float accelAbs = glm::length(accel);
            float clampRatio = 1.0f;
            if (accelAbs > maxAcceleration) {
                if (maxAcceleration > 0.0001f) {
                    clampRatio = maxAcceleration / accelAbs;
                } else {
                    clampRatio = 0.0f;
                }
            }
            wakeUp |= joint->forceConstraint->setLinearAccel(accel * clampRatio * magneticRadiusScale);
            Logf("%s: DeltaPos %s VCurr %s VMax %f VTarget %s Force %s Ratio %f",
                std::to_string(((ActorUserData *)actor->userData)->entity),
                glm::to_string(deltaPos),
                glm::to_string(PxVec3ToGlmVec3(dynamic->getLinearVelocity())),
                maxVelocity,
                glm::to_string(targetLinearVelocity),
                glm::to_string(accel * clampRatio),
                clampRatio);
        } else {
            wakeUp |= joint->forceConstraint->setLinearAccel(glm::vec3(0));
        }

        if (joint->ecsJoint.type == ecs::PhysicsJointType::Force) {
            wakeUp |= joint->forceConstraint->setGravity(gravity);
        } else {
            wakeUp |= joint->forceConstraint->setGravity(glm::vec3(0));
        }

        if (targetTransform != joint->forceConstraint->targetTransform) {
            joint->forceConstraint->targetTransform = targetTransform;
            wakeUp = true;
        }
        return wakeUp;
    }

    void ConstraintSystem::Frame(ecs::Lock<
        ecs::Read<ecs::TransformTree, ecs::CharacterController, ecs::Physics, ecs::PhysicsJoints, ecs::SceneInfo>>
            lock) {
        ZoneScoped;
        for (auto &entity : lock.EntitiesWith<ecs::Physics>()) {
            if (!entity.Has<ecs::Physics, ecs::TransformTree>(lock)) continue;
            if (manager.actors.count(entity) == 0) continue;

            auto transform = entity.Get<ecs::TransformTree>(lock).GetGlobalTransform(lock);
            auto &physics = entity.Get<ecs::Physics>(lock);
            auto const &actor = manager.actors[entity];

            UpdateJoints(lock, entity, actor, transform);

            if (physics.constantForce != glm::vec3()) {
                auto rotation = entity.Get<ecs::TransformTree>(lock).GetGlobalRotation(lock);
                auto dynamic = actor->is<PxRigidDynamic>();
                if (dynamic) dynamic->addForce(GlmVec3ToPxVec3(rotation * physics.constantForce));
            }
        }
    }

    void ConstraintSystem::ReleaseJoints(ecs::Entity entity, physx::PxRigidActor *actor) {
        if (manager.joints.count(entity) == 0) return;

        for (auto &joint : manager.joints[entity]) {
            if (joint.pxJoint) joint.pxJoint->release();
            if (joint.forceConstraint) joint.forceConstraint->release();
        }

        auto dynamic = actor->is<PxRigidDynamic>();
        if (dynamic && !dynamic->getRigidBodyFlags().isSet(PxRigidBodyFlag::eKINEMATIC)) dynamic->wakeUp();

        manager.joints.erase(entity);
    }

    void ConstraintSystem::UpdateJoints(
        ecs::Lock<ecs::Read<ecs::TransformTree, ecs::PhysicsJoints, ecs::SceneInfo>> lock,
        ecs::Entity entity,
        physx::PxRigidActor *actor,
        ecs::Transform transform) {
        if (!entity.Has<ecs::PhysicsJoints>(lock)) {
            ReleaseJoints(entity, actor);
            return;
        }

        auto &ecsJoints = entity.Get<ecs::PhysicsJoints>(lock).joints;
        if (ecsJoints.empty()) {
            ReleaseJoints(entity, actor);
            return;
        }

        bool wakeUp = false;
        auto &pxJoints = manager.joints[entity];

        sp::erase_if(pxJoints, [&](auto &pxJoint) {
            for (auto &ecsJoint : ecsJoints) {
                if (pxJoint.ecsJoint.target == ecsJoint.target && pxJoint.ecsJoint.type == ecsJoint.type) {
                    return false;
                }
            }
            if (pxJoint.pxJoint) pxJoint.pxJoint->release();
            if (pxJoint.forceConstraint) pxJoint.forceConstraint->release();
            wakeUp = true;
            return true;
        });

        ecs::SceneProperties sceneProperties = {};
        if (entity.Has<ecs::SceneInfo>(lock)) {
            auto &properties = entity.Get<ecs::SceneInfo>(lock).properties;
            if (properties) sceneProperties = *properties;
        }
        auto gravity = sceneProperties.GetGravity(transform.GetPosition());

        for (auto &ecsJoint : ecsJoints) {
            JointState *joint = nullptr;

            for (auto &j : pxJoints) {
                if (j.ecsJoint.target == ecsJoint.target && j.ecsJoint.type == ecsJoint.type) {
                    joint = &j;
                    break;
                }
            }

            physx::PxRigidActor *targetActor = nullptr;
            PxTransform localTransform(GlmVec3ToPxVec3(transform.GetScale() * ecsJoint.localOffset),
                GlmQuatToPxQuat(ecsJoint.localOrient));
            PxTransform remoteTransform(PxIdentity);
            auto targetEntity = ecsJoint.target.Get(lock);

            ecs::Transform targetTransform;
            if (manager.actors.count(targetEntity) > 0) {
                targetActor = manager.actors[targetEntity];
                auto userData = (ActorUserData *)targetActor->userData;
                Assert(userData, "Physics targetActor is missing UserData");
                remoteTransform.p = GlmVec3ToPxVec3(userData->scale * ecsJoint.remoteOffset);
                remoteTransform.q = GlmQuatToPxQuat(ecsJoint.remoteOrient);
                auto targetPose = targetActor->getGlobalPose();
                targetTransform = ecs::Transform(PxVec3ToGlmVec3(targetPose.p), PxQuatToGlmQuat(targetPose.q));
                targetTransform.Translate(glm::mat3(targetTransform.matrix) * ecsJoint.remoteOffset);
                targetTransform.Rotate(ecsJoint.remoteOrient);
            } else if (targetEntity.Has<ecs::TransformTree>(lock)) {
                targetTransform = targetEntity.Get<ecs::TransformTree>(lock).GetGlobalTransform(lock);
                targetTransform.Translate(glm::mat3(targetTransform.matrix) * ecsJoint.remoteOffset);
                targetTransform.Rotate(ecsJoint.remoteOrient);
                remoteTransform.p = GlmVec3ToPxVec3(targetTransform.GetPosition());
                remoteTransform.q = GlmQuatToPxQuat(targetTransform.GetRotation());
            }

            auto currentTransform = transform;
            currentTransform.Translate(glm::mat3(currentTransform.matrix) * ecsJoint.localOffset);
            currentTransform.Rotate(ecsJoint.localOrient);

            float intervalSeconds = manager.interval.count() / 1e9;

            // Try and determine the velocity of the joint target entity
            glm::vec3 targetVelocity(0);
            auto targetRoot = targetEntity;
            while (targetRoot.Has<ecs::TransformTree>(lock)) {
                if (manager.actors.count(targetRoot) > 0) {
                    auto userData = (ActorUserData *)manager.actors[targetRoot]->userData;
                    if (userData) {
                        targetVelocity = userData->velocity;
                        targetTransform.Translate(-targetVelocity * intervalSeconds);
                    }
                    break;
                } else if (manager.controllers.count(targetRoot) > 0) {
                    auto userData = (CharacterControllerUserData *)manager.controllers[targetRoot]->getUserData();
                    if (userData) targetVelocity = userData->actorData.velocity;
                    break;
                }
                targetRoot = targetRoot.Get<ecs::TransformTree>(lock).parent.Get(lock);
            }

            if (joint == nullptr) {
                joint = &pxJoints.emplace_back();
                joint->ecsJoint = ecsJoint;
                switch (ecsJoint.type) {
                case ecs::PhysicsJointType::Fixed:
                    joint->pxJoint =
                        PxFixedJointCreate(*manager.pxPhysics, actor, localTransform, targetActor, remoteTransform);
                    break;
                case ecs::PhysicsJointType::Distance:
                    joint->pxJoint =
                        PxDistanceJointCreate(*manager.pxPhysics, actor, localTransform, targetActor, remoteTransform);
                    break;
                case ecs::PhysicsJointType::Spherical:
                    joint->pxJoint =
                        PxSphericalJointCreate(*manager.pxPhysics, actor, localTransform, targetActor, remoteTransform);
                    break;
                case ecs::PhysicsJointType::Hinge:
                    joint->pxJoint =
                        PxRevoluteJointCreate(*manager.pxPhysics, actor, localTransform, targetActor, remoteTransform);
                    break;
                case ecs::PhysicsJointType::Slider:
                    joint->pxJoint =
                        PxPrismaticJointCreate(*manager.pxPhysics, actor, localTransform, targetActor, remoteTransform);
                    break;
                case ecs::PhysicsJointType::Force:
                case ecs::PhysicsJointType::Magnetic:
                    // Free'd automatically on release();
                    joint->forceConstraint =
                        new ForceConstraint(*manager.pxPhysics, actor, localTransform, targetActor, remoteTransform);
                    UpdateForceConstraint(actor, joint, currentTransform, targetTransform, targetVelocity, gravity);
                    break;
                default:
                    Abortf("Unsupported PhysX joint type: %s", ecsJoint.type);
                }
            } else {
                if (joint->pxJoint) {
                    if (joint->pxJoint->getLocalPose(PxJointActorIndex::eACTOR0) != localTransform) {
                        wakeUp = true;
                        joint->pxJoint->setLocalPose(PxJointActorIndex::eACTOR0, localTransform);
                    }
                    if (joint->pxJoint->getLocalPose(PxJointActorIndex::eACTOR1) != remoteTransform) {
                        wakeUp = true;
                        joint->pxJoint->setLocalPose(PxJointActorIndex::eACTOR1, remoteTransform);
                    }
                } else if (joint->forceConstraint) {
                    if (joint->forceConstraint->getLocalPose(PxJointActorIndex::eACTOR0) != localTransform) {
                        wakeUp = true;
                        joint->forceConstraint->setLocalPose(PxJointActorIndex::eACTOR0, localTransform);
                    }
                    if (joint->forceConstraint->getLocalPose(PxJointActorIndex::eACTOR1) != remoteTransform) {
                        wakeUp = true;
                        joint->forceConstraint->setLocalPose(PxJointActorIndex::eACTOR1, remoteTransform);
                    }

                    wakeUp |=
                        UpdateForceConstraint(actor, joint, currentTransform, targetTransform, targetVelocity, gravity);
                }

                if (ecsJoint == joint->ecsJoint) {
                    // joint is up to date
                    continue;
                } else {
                    joint->ecsJoint = ecsJoint;
                }
            }

            wakeUp = true;
            if (joint->pxJoint) joint->pxJoint->setActors(actor, targetActor);
            if (joint->forceConstraint) joint->forceConstraint->setActors(actor, targetActor);
            if (ecsJoint.type == ecs::PhysicsJointType::Distance) {
                auto distanceJoint = joint->pxJoint->is<PxDistanceJoint>();
                distanceJoint->setMinDistance(ecsJoint.limit.x);
                if (ecsJoint.limit.y > ecsJoint.limit.x) {
                    distanceJoint->setMaxDistance(ecsJoint.limit.y);
                    distanceJoint->setDistanceJointFlag(PxDistanceJointFlag::eMAX_DISTANCE_ENABLED, true);
                }
            } else if (ecsJoint.type == ecs::PhysicsJointType::Spherical) {
                auto sphericalJoint = joint->pxJoint->is<PxSphericalJoint>();
                if (ecsJoint.limit.x != 0.0f || ecsJoint.limit.y != 0.0f) {
                    sphericalJoint->setLimitCone(
                        PxJointLimitCone(glm::radians(ecsJoint.limit.x), glm::radians(ecsJoint.limit.y)));
                    sphericalJoint->setSphericalJointFlag(PxSphericalJointFlag::eLIMIT_ENABLED, true);
                    sphericalJoint->setConstraintFlag(PxConstraintFlag::eENABLE_EXTENDED_LIMITS, true);
                }
            } else if (ecsJoint.type == ecs::PhysicsJointType::Hinge) {
                auto revoluteJoint = joint->pxJoint->is<PxRevoluteJoint>();
                if (ecsJoint.limit.x != 0.0f || ecsJoint.limit.y != 0.0f) {
                    revoluteJoint->setLimit(
                        PxJointAngularLimitPair(glm::radians(ecsJoint.limit.x), glm::radians(ecsJoint.limit.y)));
                    revoluteJoint->setRevoluteJointFlag(PxRevoluteJointFlag::eLIMIT_ENABLED, true);
                    revoluteJoint->setConstraintFlag(PxConstraintFlag::eENABLE_EXTENDED_LIMITS, true);
                }
            } else if (ecsJoint.type == ecs::PhysicsJointType::Slider) {
                auto prismaticJoint = joint->pxJoint->is<PxPrismaticJoint>();
                if (ecsJoint.limit.x != 0.0f || ecsJoint.limit.y != 0.0f) {
                    prismaticJoint->setLimit(PxJointLinearLimitPair(manager.pxPhysics->getTolerancesScale(),
                        ecsJoint.limit.x,
                        ecsJoint.limit.y));
                    prismaticJoint->setPrismaticJointFlag(PxPrismaticJointFlag::eLIMIT_ENABLED, true);
                }
            } else if (ecsJoint.type == ecs::PhysicsJointType::Force) {
                joint->forceConstraint->setForceLimits(ecsJoint.limit.x, ecsJoint.limit.x, ecsJoint.limit.y);
            } else if (ecsJoint.type == ecs::PhysicsJointType::Magnetic) {
                joint->forceConstraint->setForceLimits(ecsJoint.limit.x, ecsJoint.limit.x, ecsJoint.limit.y);
            }
        }

        if (wakeUp) {
            auto dynamic = actor->is<PxRigidDynamic>();
            if (dynamic && !dynamic->getRigidBodyFlags().isSet(PxRigidBodyFlag::eKINEMATIC)) dynamic->wakeUp();
        }
    }
} // namespace sp
