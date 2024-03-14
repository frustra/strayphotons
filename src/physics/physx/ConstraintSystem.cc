/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "ConstraintSystem.hh"

#include "common/Common.hh"
#include "common/Logging.hh"
#include "console/CVar.hh"
#include "ecs/EcsImpl.hh"
#include "input/BindingNames.hh"
#include "physx/ForceConstraint.hh"
#include "physx/NoClipConstraint.hh"
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
        glm::vec3 targetLinearVelocity,
        glm::vec3 gravity) {
        if (!actor || !joint || !joint->forceConstraint) return false;
        auto dynamic = actor->is<PxRigidDynamic>();
        if (!dynamic) return false;
        auto centerOfMass = PxVec3ToGlmVec3(dynamic->getCMassLocalPose().p);

        auto currentRotate = transform.GetRotation();
        transform.Translate(currentRotate * centerOfMass);
        auto targetRotate = targetTransform.GetRotation();
        targetTransform.Translate(targetRotate * centerOfMass);

        float intervalSeconds = manager.interval.count() / 1e9;
        float tickFrequency = 1e9 / manager.interval.count();

        float maxForce = joint->ecsJoint.limit.x;
        float maxTorque = joint->ecsJoint.limit.y;

        bool wakeUp = false;

        // Update Torque
        if (maxTorque > 0) {
            if (glm::dot(currentRotate, targetRotate) < 0.0f) { // minimum distance quaternion
                targetRotate = -targetRotate;
            }
            auto deltaRotation = glm::eulerAngles(glm::inverse(currentRotate) * targetRotate);

            // PhysX applies damping and clamps velocity before applying forces, so we need to account for this.
            auto currentAngularVelocity = PxVec3ToGlmVec3(dynamic->getAngularVelocity());
            currentAngularVelocity *= glm::max(0.0f, 1.0f - intervalSeconds * dynamic->getAngularDamping());
            auto maxAngularVelocity = dynamic->getMaxAngularVelocity();
            if (glm::length(currentAngularVelocity) > maxAngularVelocity) {
                currentAngularVelocity *= maxAngularVelocity / glm::length(currentAngularVelocity);
            }
            currentAngularVelocity = glm::inverse(currentRotate) * currentAngularVelocity;

            auto maxAcceleration = PxVec3ToGlmVec3(dynamic->getMassSpaceInvInertiaTensor()) * maxTorque;
            auto maxDeltaVelocity = maxAcceleration * intervalSeconds;
            glm::vec3 accel;
            for (int i = 0; i < 3; i++) {
                auto targetDist = std::abs(deltaRotation[i]);
                // Maximum velocity achievable over deltaPos distance (also max velocity we can decelerate from)
                auto maxVelocity = std::sqrt(2 * maxAcceleration[i] * targetDist);
                if (maxVelocity > maxAngularVelocity) {
                    maxVelocity = maxAngularVelocity;
                }
                if (targetDist < maxVelocity * intervalSeconds) {
                    maxVelocity = targetDist * tickFrequency;
                }

                auto deltaAccelVelocity = (glm::sign(deltaRotation[i]) * maxVelocity) - currentAngularVelocity[i];
                if (std::abs(deltaAccelVelocity) < maxDeltaVelocity[i]) {
                    accel[i] = deltaAccelVelocity * tickFrequency;
                } else {
                    accel[i] = glm::sign(deltaAccelVelocity) * maxAcceleration[i];
                }
            }
            wakeUp |= joint->forceConstraint->setAngularAccel(accel);
        } else {
            wakeUp |= joint->forceConstraint->setAngularAccel(glm::vec3(0));
        }

        // Update Linear Force
        if (maxForce > 0) {
            auto deltaPos = targetTransform.GetPosition() - transform.GetPosition();
            auto currentLinearVelocity = PxVec3ToGlmVec3(dynamic->getLinearVelocity());
            auto deltaVelocity = targetLinearVelocity - currentLinearVelocity;

            auto maxAcceleration = maxForce / dynamic->getMass();
            auto maxDeltaVelocity = maxAcceleration * intervalSeconds;
            glm::vec3 accel;
            if (deltaPos == glm::vec3(0)) {
                if (glm::length(deltaVelocity) < maxDeltaVelocity) {
                    accel = deltaVelocity * tickFrequency;
                } else {
                    accel = glm::normalize(deltaVelocity) * maxAcceleration;
                }
            } else {
                auto targetDist = glm::length(deltaPos);
                // Maximum velocity achievable over deltaPos distance (also max velocity we can decelerate from)
                auto maxVelocity = std::sqrt(2 * maxAcceleration * targetDist);
                if (targetDist < maxVelocity * intervalSeconds) {
                    maxVelocity = targetDist * tickFrequency;
                }

                auto deltaAccelVelocity = deltaVelocity + glm::normalize(deltaPos) * maxVelocity;
                if (glm::length(deltaAccelVelocity) < maxDeltaVelocity) {
                    accel = deltaAccelVelocity * tickFrequency;
                } else {
                    accel = glm::normalize(deltaAccelVelocity) * maxAcceleration;
                }
            }
            wakeUp |= joint->forceConstraint->setLinearAccel(accel);
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

    bool ConstraintSystem::UpdateNoClipConstraint(JointState *joint, PxRigidActor *actor0, PxRigidActor *actor1) {
        if (!joint->noclipConstraint || !joint->noclipConstraint->temporary) return false;
        if (!actor0 || !actor1) {
            Errorf("Invalid NoClip constraint has null actor: %s", joint->ecsJoint.target.Name().String());
            return true;
        }

        vector<PxShape *> shapes0(actor0->getNbShapes());
        vector<PxShape *> shapes1(actor1->getNbShapes());
        actor0->getShapes(shapes0.data(), shapes0.size());
        actor1->getShapes(shapes1.data(), shapes1.size());

        auto pose0 = actor0->getGlobalPose();
        auto pose1 = actor1->getGlobalPose();

        for (auto *shape0 : shapes0) {
            for (auto *shape1 : shapes1) {
                if (PxGeometryQuery::overlap(shape0->getGeometry().any(),
                        pose0 * shape0->getLocalPose(),
                        shape1->getGeometry().any(),
                        pose1 * shape1->getLocalPose())) {
                    return false;
                }
            }
        }

        joint->noclipConstraint->release();
        joint->noclipConstraint = nullptr;
        return true;
    }

    void ConstraintSystem::Frame(
        ecs::Lock<ecs::Read<ecs::TransformTree, ecs::CharacterController, ecs::Physics, ecs::SceneProperties>,
            ecs::Write<ecs::PhysicsJoints>> lock) {
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
        for (auto &entity : lock.EntitiesWith<ecs::CharacterController>()) {
            if (!entity.Has<ecs::CharacterController, ecs::TransformTree>(lock)) continue;

            auto transform = entity.Get<ecs::TransformTree>(lock).GetGlobalTransform(lock);
            auto &controller = entity.Get<ecs::CharacterController>(lock);
            if (!controller.pxController) continue;

            UpdateJoints(lock, entity, controller.pxController->getActor(), transform);
        }
    }

    void ConstraintSystem::ReleaseJoints(ecs::Entity entity, physx::PxRigidActor *actor) {
        if (manager.joints.count(entity) == 0) return;

        for (auto &joint : manager.joints[entity]) {
            if (joint.pxJoint) joint.pxJoint->release();
            if (joint.forceConstraint) joint.forceConstraint->release();
            if (joint.noclipConstraint) joint.noclipConstraint->release();
        }

        if (actor->getScene()) {
            auto dynamic = actor->is<PxRigidDynamic>();
            if (dynamic && !dynamic->getRigidBodyFlags().isSet(PxRigidBodyFlag::eKINEMATIC)) dynamic->wakeUp();
        }

        manager.joints.erase(entity);
    }

    void ConstraintSystem::UpdateJoints(
        ecs::Lock<ecs::Read<ecs::TransformTree, ecs::SceneProperties>, ecs::Write<ecs::PhysicsJoints>> lock,
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
            if (pxJoint.noclipConstraint) pxJoint.noclipConstraint->release();
            wakeUp = true;
            return true;
        });

        auto &sceneProperties = ecs::SceneProperties::Get(lock, entity);
        auto gravity = sceneProperties.GetGravity(transform.GetPosition());

        // For each defined ecs joint, removing temporary joints as needed.
        sp::erase_if(ecsJoints, [&](auto &ecsJoint) {
            JointState *joint = nullptr;

            for (auto &j : pxJoints) {
                if (j.ecsJoint.target == ecsJoint.target && j.ecsJoint.type == ecsJoint.type) {
                    joint = &j;
                    break;
                }
            }

            physx::PxRigidActor *targetActor = nullptr;
            PxTransform localTransform(GlmVec3ToPxVec3(transform.GetScale() * ecsJoint.localOffset.GetPosition()),
                GlmQuatToPxQuat(ecsJoint.localOffset.GetRotation()));
            PxTransform remoteTransform(PxIdentity);
            ecs::Entity targetEntity = ecsJoint.target.Get(lock);

            ecs::Transform targetTransform;
            if (manager.actors.count(targetEntity) > 0) {
                targetActor = manager.actors[targetEntity];
                auto userData = (ActorUserData *)targetActor->userData;
                Assert(userData, "Physics targetActor is missing UserData");
                remoteTransform.p = GlmVec3ToPxVec3(userData->scale * ecsJoint.remoteOffset.GetPosition());
                remoteTransform.q = GlmQuatToPxQuat(ecsJoint.remoteOffset.GetRotation());
                auto targetPose = targetActor->getGlobalPose();
                targetTransform = ecs::Transform(PxVec3ToGlmVec3(targetPose.p), PxQuatToGlmQuat(targetPose.q));
                targetTransform.Translate(targetTransform * glm::vec4(ecsJoint.remoteOffset.GetPosition(), 0));
                targetTransform.Rotate(ecsJoint.remoteOffset.GetRotation());
            } else if (manager.subActors.count(targetEntity) > 0) {
                targetActor = manager.subActors[targetEntity];
                auto userData = (ActorUserData *)targetActor->userData;
                Assert(userData, "Physics targetActor is missing UserData");
                remoteTransform.p = GlmVec3ToPxVec3(userData->scale * ecsJoint.remoteOffset.GetPosition());
                remoteTransform.q = GlmQuatToPxQuat(ecsJoint.remoteOffset.GetRotation());

                if (targetEntity.Has<ecs::TransformTree>(lock)) {
                    auto subActorTransform = targetEntity.Get<ecs::TransformTree>(lock).GetRelativeTransform(lock,
                        userData->entity);
                    remoteTransform.p += GlmVec3ToPxVec3(userData->scale * subActorTransform.GetPosition());
                    remoteTransform.q *= GlmQuatToPxQuat(subActorTransform.GetRotation());
                }

                auto targetPose = targetActor->getGlobalPose();
                targetTransform = ecs::Transform(PxVec3ToGlmVec3(targetPose.p), PxQuatToGlmQuat(targetPose.q));
                targetTransform.Translate(targetTransform * glm::vec4(ecsJoint.remoteOffset.GetPosition(), 0));
                targetTransform.Rotate(ecsJoint.remoteOffset.GetRotation());
            } else {
                if (targetEntity.Has<ecs::TransformTree>(lock)) {
                    targetTransform = targetEntity.Get<ecs::TransformTree>(lock).GetGlobalTransform(lock);
                } // else Target is scene root
                targetTransform.Translate(targetTransform * glm::vec4(ecsJoint.remoteOffset.GetPosition(), 0));
                targetTransform.Rotate(ecsJoint.remoteOffset.GetRotation());
                remoteTransform.p = GlmVec3ToPxVec3(targetTransform.GetPosition());
                remoteTransform.q = GlmQuatToPxQuat(targetTransform.GetRotation());
            }

            auto currentTransform = transform;
            currentTransform.Translate(currentTransform * glm::vec4(ecsJoint.localOffset.GetPosition(), 0));
            currentTransform.Rotate(ecsJoint.localOffset.GetRotation());

            float intervalSeconds = manager.interval.count() / 1e9;

            // Try and determine the velocity of the joint target entity
            glm::vec3 targetVelocity(0);
            ecs::Entity targetRoot = targetEntity;
            while (targetRoot.Has<ecs::TransformTree>(lock)) {
                if (manager.actors.count(targetRoot) > 0) {
                    auto userData = (ActorUserData *)manager.actors[targetRoot]->userData;
                    if (userData) {
                        targetVelocity = userData->velocity;
                        targetTransform.Translate(-targetVelocity * intervalSeconds);
                    }
                    break;
                } else if (manager.subActors.count(targetRoot) > 0) {
                    auto userData = (ActorUserData *)manager.subActors[targetRoot]->userData;
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
                    joint->pxJoint = PxFixedJointCreate(*manager.pxPhysics,
                        actor,
                        localTransform,
                        targetActor,
                        remoteTransform);
                    break;
                case ecs::PhysicsJointType::Distance:
                    joint->pxJoint = PxDistanceJointCreate(*manager.pxPhysics,
                        actor,
                        localTransform,
                        targetActor,
                        remoteTransform);
                    break;
                case ecs::PhysicsJointType::Spherical:
                    joint->pxJoint = PxSphericalJointCreate(*manager.pxPhysics,
                        actor,
                        localTransform,
                        targetActor,
                        remoteTransform);
                    break;
                case ecs::PhysicsJointType::Hinge:
                    joint->pxJoint = PxRevoluteJointCreate(*manager.pxPhysics,
                        actor,
                        localTransform,
                        targetActor,
                        remoteTransform);
                    break;
                case ecs::PhysicsJointType::Slider:
                    joint->pxJoint = PxPrismaticJointCreate(*manager.pxPhysics,
                        actor,
                        localTransform,
                        targetActor,
                        remoteTransform);
                    break;
                case ecs::PhysicsJointType::Force:
                    // Free'd automatically on release()
                    joint->forceConstraint = new ForceConstraint(*manager.pxPhysics,
                        actor,
                        localTransform,
                        targetActor,
                        remoteTransform);
                    UpdateForceConstraint(actor, joint, currentTransform, targetTransform, targetVelocity, gravity);
                    break;
                case ecs::PhysicsJointType::NoClip:
                case ecs::PhysicsJointType::TemporaryNoClip:
                    // Free'd automatically on release()
                    joint->noclipConstraint = new NoClipConstraint(*manager.pxPhysics,
                        actor,
                        targetActor,
                        ecsJoint.type == ecs::PhysicsJointType::TemporaryNoClip);
                    if (UpdateNoClipConstraint(joint, actor, targetActor)) {
                        return true;
                    }
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

                    wakeUp |= UpdateForceConstraint(actor,
                        joint,
                        currentTransform,
                        targetTransform,
                        targetVelocity,
                        gravity);
                } else if (joint->noclipConstraint) {
                    if (UpdateNoClipConstraint(joint, actor, targetActor)) {
                        return true;
                    }
                }

                if (ecsJoint == joint->ecsJoint) {
                    // joint is up to date
                    return false;
                } else {
                    joint->ecsJoint = ecsJoint;
                }
            }

            wakeUp = true;
            if (joint->pxJoint) joint->pxJoint->setActors(actor, targetActor);
            if (joint->forceConstraint) joint->forceConstraint->setActors(actor, targetActor);
            if (joint->noclipConstraint) joint->noclipConstraint->setActors(actor, targetActor);
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
            }
            return false;
        });

        if (wakeUp && actor->getScene()) {
            auto dynamic = actor->is<PxRigidDynamic>();
            if (dynamic && !dynamic->getRigidBodyFlags().isSet(PxRigidBodyFlag::eKINEMATIC)) dynamic->wakeUp();
        }
    }
} // namespace sp
