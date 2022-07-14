#include "ConstraintSystem.hh"

#include "console/CVar.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/Ecs.hh"
#include "ecs/EcsImpl.hh"
#include "input/BindingNames.hh"
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

    void ConstraintSystem::BreakConstraints(ecs::Lock<ecs::Read<ecs::TransformSnapshot, ecs::CharacterController>,
        ecs::Write<ecs::Physics>,
        ecs::SendEventsLock> lock) {
        ZoneScoped;

        InlineVector<ecs::Entity, 32> breakList;
        for (auto &entity : lock.EntitiesWith<ecs::CharacterController>()) {
            auto &controller = entity.Get<ecs::CharacterController>(lock);
            if (controller.pxController) {
                auto userData = (CharacterControllerUserData *)controller.pxController->getUserData();
                if (userData && userData->standingOn) breakList.push_back(userData->standingOn);
            }
        }

        for (auto &entity : lock.EntitiesWith<ecs::Physics>()) {
            if (!entity.Has<ecs::Physics, ecs::TransformSnapshot>(lock)) continue;
            auto &physics = entity.Get<ecs::Physics>(lock);
            auto constraintEntity = physics.constraint.Get(lock);
            if (sp::contains(breakList, entity)) {
                ecs::EventBindings::SendEvent(lock, PHYSICS_EVENT_BROKEN_CONSTRAINT, entity, physics.constraint);
                physics.RemoveConstraint();
            } else if (physics.constraintMaxDistance > 0.0f && constraintEntity.Has<ecs::TransformSnapshot>(lock)) {
                auto &transform = entity.Get<ecs::TransformSnapshot>(lock);
                auto &targetTransform = constraintEntity.Get<ecs::TransformSnapshot>(lock);

                auto deltaPos = targetTransform.GetPosition() - transform.GetPosition() +
                                targetTransform.GetRotation() * physics.constraintOffset;
                if (glm::length(deltaPos) > physics.constraintMaxDistance) {
                    ecs::EventBindings::SendEvent(lock, PHYSICS_EVENT_BROKEN_CONSTRAINT, entity, physics.constraint);
                    physics.RemoveConstraint();
                }
            }
        }
    }

    void ConstraintSystem::Frame(
        ecs::Lock<ecs::Read<ecs::TransformTree, ecs::CharacterController, ecs::Physics, ecs::PhysicsJoints>> lock) {
        ZoneScoped;
        for (auto &entity : lock.EntitiesWith<ecs::Physics>()) {
            if (!entity.Has<ecs::Physics, ecs::TransformTree>(lock)) continue;
            if (manager.actors.count(entity) == 0) continue;

            auto transform = entity.Get<ecs::TransformTree>(lock).GetGlobalTransform(lock);
            auto &physics = entity.Get<ecs::Physics>(lock);
            auto const &actor = manager.actors[entity];

            UpdateJoints(lock, entity, actor, transform);

            auto constraintEntity = physics.constraint.Get(lock);
            if (constraintEntity.Has<ecs::TransformTree>(lock)) {
                auto &targetTransform = constraintEntity.Get<ecs::TransformTree>(lock);
                glm::vec3 targetVelocity(0);

                // Try and determine the velocity of the constraint target entity
                while (constraintEntity.Has<ecs::TransformTree>(lock)) {
                    if (manager.actors.count(constraintEntity) > 0) {
                        auto userData = (ActorUserData *)manager.actors[constraintEntity]->userData;
                        if (userData) targetVelocity = userData->velocity;
                        break;
                    } else if (constraintEntity.Has<ecs::CharacterController>(lock)) {
                        auto &targetController = constraintEntity.Get<ecs::CharacterController>(lock);
                        if (targetController.pxController) {
                            auto userData = (CharacterControllerUserData *)targetController.pxController->getUserData();
                            if (userData) targetVelocity = userData->actorData.velocity;
                            break;
                        }
                    }
                    constraintEntity = constraintEntity.Get<ecs::TransformTree>(lock).parent.Get(lock);
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

    void ConstraintSystem::ReleaseJoints(ecs::Entity entity, physx::PxRigidActor *actor) {
        if (manager.joints.count(entity) == 0) return;

        for (auto joint : manager.joints[entity]) {
            joint.pxJoint->release();
        }

        auto dynamic = actor->is<PxRigidDynamic>();
        if (dynamic && !dynamic->getRigidBodyFlags().isSet(PxRigidBodyFlag::eKINEMATIC)) dynamic->wakeUp();

        manager.joints.erase(entity);
    }

    void ConstraintSystem::UpdateJoints(ecs::Lock<ecs::Read<ecs::TransformTree, ecs::PhysicsJoints>> lock,
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

        for (auto it = pxJoints.begin(); it != pxJoints.end();) {
            bool matchingJoint = false;
            for (auto &ecsJoint : ecsJoints) {
                if (it->ecsJoint.target == ecsJoint.target && it->ecsJoint.type == ecsJoint.type) {
                    matchingJoint = true;
                    break;
                }
            }
            if (matchingJoint) {
                it++;
            } else {
                it->pxJoint->release();
                it = pxJoints.erase(it);
                wakeUp = true;
            }
        }

        for (auto &ecsJoint : ecsJoints) {
            ecs::PhysicsJoint *oldEcsJoint = nullptr;
            physx::PxJoint *pxJoint = nullptr;

            for (auto &j : pxJoints) {
                if (j.ecsJoint.target == ecsJoint.target && j.ecsJoint.type == ecsJoint.type) {
                    pxJoint = j.pxJoint;
                    oldEcsJoint = &j.ecsJoint;
                    break;
                }
            }

            physx::PxRigidActor *targetActor = nullptr;
            PxTransform localTransform(GlmVec3ToPxVec3(transform.GetScale() * ecsJoint.localOffset),
                GlmQuatToPxQuat(ecsJoint.localOrient));
            PxTransform remoteTransform(PxIdentity);
            auto targetEntity = ecsJoint.target.Get(lock);

            if (manager.actors.count(targetEntity) > 0) {
                targetActor = manager.actors[targetEntity];
                auto userData = (ActorUserData *)targetActor->userData;
                Assert(userData, "Physics targetActor is missing UserData");
                remoteTransform.p = GlmVec3ToPxVec3(userData->scale * ecsJoint.remoteOffset);
                remoteTransform.q = GlmQuatToPxQuat(ecsJoint.remoteOrient);
            }
            if (!targetActor && targetEntity.Has<ecs::TransformTree>(lock)) {
                auto targetTransform = targetEntity.Get<ecs::TransformTree>(lock).GetGlobalTransform(lock);
                remoteTransform.p = GlmVec3ToPxVec3(
                    targetTransform.GetPosition() + glm::mat3(targetTransform.matrix) * ecsJoint.remoteOffset);
                remoteTransform.q = GlmQuatToPxQuat(targetTransform.GetRotation() * ecsJoint.remoteOrient);
            }

            if (pxJoint == nullptr) {
                switch (ecsJoint.type) {
                case ecs::PhysicsJointType::Fixed:
                    pxJoint =
                        PxFixedJointCreate(*manager.pxPhysics, actor, localTransform, targetActor, remoteTransform);
                    break;
                case ecs::PhysicsJointType::Distance:
                    pxJoint =
                        PxDistanceJointCreate(*manager.pxPhysics, actor, localTransform, targetActor, remoteTransform);
                    break;
                case ecs::PhysicsJointType::Spherical:
                    pxJoint =
                        PxSphericalJointCreate(*manager.pxPhysics, actor, localTransform, targetActor, remoteTransform);
                    break;
                case ecs::PhysicsJointType::Hinge:
                    pxJoint =
                        PxRevoluteJointCreate(*manager.pxPhysics, actor, localTransform, targetActor, remoteTransform);
                    break;
                case ecs::PhysicsJointType::Slider:
                    pxJoint =
                        PxPrismaticJointCreate(*manager.pxPhysics, actor, localTransform, targetActor, remoteTransform);
                    break;
                default:
                    Abortf("Unsupported PhysX joint type: %u", ecsJoint.type);
                }
                pxJoints.emplace_back(PhysxManager::Joint{ecsJoint, pxJoint});
            } else {
                if (pxJoint->getLocalPose(PxJointActorIndex::eACTOR0) != localTransform) {
                    wakeUp = true;
                    pxJoint->setLocalPose(PxJointActorIndex::eACTOR0, localTransform);
                }
                if (pxJoint->getLocalPose(PxJointActorIndex::eACTOR1) != remoteTransform) {
                    wakeUp = true;
                    pxJoint->setLocalPose(PxJointActorIndex::eACTOR1, remoteTransform);
                }

                if (ecsJoint == *oldEcsJoint) {
                    // joint is up to date
                    continue;
                } else {
                    *oldEcsJoint = ecsJoint;
                }
            }

            wakeUp = true;
            pxJoint->setActors(actor, targetActor);
            if (ecsJoint.type == ecs::PhysicsJointType::Distance) {
                auto distanceJoint = pxJoint->is<PxDistanceJoint>();
                distanceJoint->setMinDistance(ecsJoint.range.x);
                if (ecsJoint.range.y > ecsJoint.range.x) {
                    distanceJoint->setMaxDistance(ecsJoint.range.y);
                    distanceJoint->setDistanceJointFlag(PxDistanceJointFlag::eMAX_DISTANCE_ENABLED, true);
                }
                pxJoint = distanceJoint;
            } else if (ecsJoint.type == ecs::PhysicsJointType::Spherical) {
                auto sphericalJoint = pxJoint->is<PxSphericalJoint>();
                if (ecsJoint.range.x != 0.0f || ecsJoint.range.y != 0.0f) {
                    sphericalJoint->setLimitCone(
                        PxJointLimitCone(glm::radians(ecsJoint.range.x), glm::radians(ecsJoint.range.y)));
                    sphericalJoint->setSphericalJointFlag(PxSphericalJointFlag::eLIMIT_ENABLED, true);
                    sphericalJoint->setConstraintFlag(PxConstraintFlag::eENABLE_EXTENDED_LIMITS, true);
                }
                pxJoint = sphericalJoint;
            } else if (ecsJoint.type == ecs::PhysicsJointType::Hinge) {
                auto revoluteJoint = pxJoint->is<PxRevoluteJoint>();
                if (ecsJoint.range.x != 0.0f || ecsJoint.range.y != 0.0f) {
                    revoluteJoint->setLimit(
                        PxJointAngularLimitPair(glm::radians(ecsJoint.range.x), glm::radians(ecsJoint.range.y)));
                    revoluteJoint->setRevoluteJointFlag(PxRevoluteJointFlag::eLIMIT_ENABLED, true);
                    revoluteJoint->setConstraintFlag(PxConstraintFlag::eENABLE_EXTENDED_LIMITS, true);
                }
                pxJoint = revoluteJoint;
            } else if (ecsJoint.type == ecs::PhysicsJointType::Slider) {
                auto prismaticJoint = pxJoint->is<PxPrismaticJoint>();
                if (ecsJoint.range.x != 0.0f || ecsJoint.range.y != 0.0f) {
                    prismaticJoint->setLimit(PxJointLinearLimitPair(manager.pxPhysics->getTolerancesScale(),
                        ecsJoint.range.x,
                        ecsJoint.range.y));
                    prismaticJoint->setPrismaticJointFlag(PxPrismaticJointFlag::eLIMIT_ENABLED, true);
                }
                pxJoint = prismaticJoint;
            }
        }

        if (wakeUp) {
            auto dynamic = actor->is<PxRigidDynamic>();
            if (dynamic && !dynamic->getRigidBodyFlags().isSet(PxRigidBodyFlag::eKINEMATIC)) dynamic->wakeUp();
        }
    }
} // namespace sp
