/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "SimulationCallbackHandler.hh"

#include "common/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "input/BindingNames.hh"
#include "physx/PhysxManager.hh"
#include "physx/PhysxUtils.hh"

#include <glm/gtx/norm.hpp>

namespace sp {
    using namespace physx;

    // Constraint break events are received for all constraints, no registering is required
    void SimulationCallbackHandler::onConstraintBreak(PxConstraintInfo *constraints, PxU32 count) {
        Logf("SimulationCallbackHandler::onConstraintBreak: %u", count);
    }

    // Sleep/Wake events require an actor have the PxActorFlag::eSEND_SLEEP_NOTIFIES flag
    void SimulationCallbackHandler::onWake(PxActor **actors, PxU32 count) {
        Logf("SimulationCallbackHandler::onWake: %u", count);
    }
    void SimulationCallbackHandler::onSleep(PxActor **actors, PxU32 count) {
        Logf("SimulationCallbackHandler::onSleep: %u", count);
    }

    // Contact events require an actor pair to have PxPairFlag::eNOTIFY_TOUCH_FOUND, PxPairFlag::eNOTIFY_TOUCH_PERSISTS,
    // or PxPairFlag::eNOTIFY_TOUCH_LOST flags (or the THRESHOLD_FORCE variants) set in the simulation shader.
    void SimulationCallbackHandler::onContact(const PxContactPairHeader &pairHeader,
        const PxContactPair *pairs,
        PxU32 nbPairs) {
        if (nbPairs == 0) return;
        if (pairHeader.flags.isSet(PxContactPairHeaderFlag::eREMOVED_ACTOR_0) ||
            pairHeader.flags.isSet(PxContactPairHeaderFlag::eREMOVED_ACTOR_1)) {
            return;
        }
        if (!pairHeader.actors[0] || !pairHeader.actors[1]) return;
        auto *userData0 = (ActorUserData *)pairHeader.actors[0]->userData;
        auto *userData1 = (ActorUserData *)pairHeader.actors[1]->userData;
        if (!userData0 || !userData1) return;
        auto thresholdForce0 = userData0->contactReportThreshold >= 0 ? userData0->contactReportThreshold : PX_MAX_F32;
        auto thresholdForce1 = userData1->contactReportThreshold >= 0 ? userData1->contactReportThreshold : PX_MAX_F32;
        float thresholdForce = std::min(thresholdForce0, thresholdForce1);

        auto lock = ecs::StartTransaction<ecs::SendEventsLock>();

        PxContactPairExtraDataIterator iter(pairHeader.extraDataStream, pairHeader.extraDataStreamSize);
        while (iter.nextItemSet()) {
            if (!iter.preSolverVelocity || !iter.postSolverVelocity) continue;

            auto &pair = pairs[iter.contactPairIndex];
            if (!pair.shapes[0] || !pair.shapes[1]) continue;
            auto *shapeUserData0 = (ShapeUserData *)pair.shapes[0]->userData;
            auto *shapeUserData1 = (ShapeUserData *)pair.shapes[1]->userData;
            if (!shapeUserData0 || !shapeUserData1) continue;
            if (shapeUserData0->parent != userData0->entity || shapeUserData1->parent != userData1->entity) {
                continue;
            }

            // Debugf("onContact: %s - %s",
            //     ecs::ToString(lock, shapeUserData0->parent),
            //     ecs::ToString(lock, shapeUserData1->parent));

            const float frameRate = 120.0f;
            float maxForce = 0.0f;

            const auto *dynamicActor0 = pairHeader.actors[0]->is<PxRigidDynamic>();
            if (dynamicActor0) {
                glm::vec3 preSolveLinear0 = PxVec3ToGlmVec3(iter.preSolverVelocity->linearVelocity[0]);
                glm::vec3 postSolveLinear0 = PxVec3ToGlmVec3(iter.postSolverVelocity->linearVelocity[0]);
                glm::vec3 deltaLinear0 = postSolveLinear0 - preSolveLinear0;

                glm::vec3 preSolveAngular0 = PxVec3ToGlmVec3(iter.preSolverVelocity->angularVelocity[0]);
                glm::vec3 postSolveAngular0 = PxVec3ToGlmVec3(iter.postSolverVelocity->angularVelocity[0]);
                glm::vec3 deltaAngular0 = postSolveAngular0 - preSolveAngular0;

                float linearForce = glm::length(deltaLinear0) * dynamicActor0->getMass();
                float angularTorque = glm::length(
                    deltaAngular0 * PxVec3ToGlmVec3(dynamicActor0->getMassSpaceInertiaTensor()));
                if (linearForce > 0 || angularTorque > 0) {
                    // Logf("Actor0 %s shape %u mass %f Linear %f Angular %f",
                    //     ecs::EntityRef(userData0->entity).Name().String(),
                    //     iter.contactPairIndex,
                    //     dynamicActor0->getMass(),
                    //     linearForce * frameRate,
                    //     angularTorque * frameRate);
                    maxForce = std::max({maxForce, linearForce * frameRate, angularTorque * frameRate});
                }
            }
            const auto *dynamicActor1 = pairHeader.actors[1]->is<PxRigidDynamic>();
            if (dynamicActor1) {
                glm::vec3 preSolveLinear = PxVec3ToGlmVec3(iter.preSolverVelocity->linearVelocity[1]);
                glm::vec3 postSolveLinear = PxVec3ToGlmVec3(iter.postSolverVelocity->linearVelocity[1]);
                glm::vec3 deltaLinear = postSolveLinear - preSolveLinear;

                glm::vec3 preSolveAngular = PxVec3ToGlmVec3(iter.preSolverVelocity->angularVelocity[1]);
                glm::vec3 postSolveAngular = PxVec3ToGlmVec3(iter.postSolverVelocity->angularVelocity[1]);
                glm::vec3 deltaAngular = postSolveAngular - preSolveAngular;

                float linearForce = glm::length(deltaLinear) * dynamicActor1->getMass();
                float angularTorque = glm::length(
                    deltaAngular * PxVec3ToGlmVec3(dynamicActor1->getMassSpaceInertiaTensor()));
                if (linearForce > 0 || angularTorque > 0) {
                    // Logf("Actor1 %s shape %u mass %f Linear %f Angular %f",
                    //     ecs::EntityRef(userData1->entity).Name().String(),
                    //     iter.contactPairIndex,
                    //     dynamicActor1->getMass(),
                    //     linearForce * frameRate,
                    //     angularTorque * frameRate);
                    maxForce = std::max({maxForce, linearForce * frameRate, angularTorque * frameRate});
                }
            }

            if ((pair.events.isSet(PxPairFlag::eNOTIFY_TOUCH_FOUND) ||
                    pair.events.isSet(PxPairFlag::eNOTIFY_TOUCH_PERSISTS)) &&
                maxForce >= thresholdForce) {
                if (shapeUserData0->owner != shapeUserData0->parent) {
                    ecs::EventBindings::SendEvent(lock,
                        shapeUserData0->owner,
                        ecs::Event{PHYSICS_EVENT_COLLISION_FORCE_FOUND, shapeUserData1->parent, maxForce});
                }
                if (shapeUserData1->owner != shapeUserData1->parent) {
                    ecs::EventBindings::SendEvent(lock,
                        shapeUserData1->owner,
                        ecs::Event{PHYSICS_EVENT_COLLISION_FORCE_FOUND, shapeUserData0->parent, maxForce});
                }

                ecs::EventBindings::SendEvent(lock,
                    shapeUserData0->parent,
                    ecs::Event{PHYSICS_EVENT_COLLISION_FORCE_FOUND, shapeUserData1->parent, maxForce});
                ecs::EventBindings::SendEvent(lock,
                    shapeUserData1->parent,
                    ecs::Event{PHYSICS_EVENT_COLLISION_FORCE_FOUND, shapeUserData0->parent, maxForce});
            } else if (pair.events.isSet(PxPairFlag::eNOTIFY_TOUCH_LOST)) {
                ecs::EventBindings::SendEvent(lock,
                    shapeUserData0->parent,
                    ecs::Event{PHYSICS_EVENT_COLLISION_FORCE_LOST, shapeUserData1->parent, maxForce});
                ecs::EventBindings::SendEvent(lock,
                    shapeUserData1->parent,
                    ecs::Event{PHYSICS_EVENT_COLLISION_FORCE_LOST, shapeUserData0->parent, maxForce});

                if (shapeUserData0->owner != shapeUserData0->parent) {
                    ecs::EventBindings::SendEvent(lock,
                        shapeUserData0->owner,
                        ecs::Event{PHYSICS_EVENT_COLLISION_FORCE_LOST, shapeUserData1->parent, maxForce});
                }
                if (shapeUserData1->owner != shapeUserData1->parent) {
                    ecs::EventBindings::SendEvent(lock,
                        shapeUserData1->owner,
                        ecs::Event{PHYSICS_EVENT_COLLISION_FORCE_LOST, shapeUserData0->parent, maxForce});
                }
            }
        }
    }

    // Trigger events require an actor to have the PxShapeFlag::eTRIGGER_SHAPE simulation flag.
    void SimulationCallbackHandler::onTrigger(PxTriggerPair *pairs, PxU32 count) {
        Logf("SimulationCallbackHandler::onTrigger: %u", count);
    }

    // Called for rigid bodies that have moved and have the PxRigidBodyFlag::eENABLE_POSE_INTEGRATION_PREVIEW flag
    // set. This callback is invoked inline with the simulation and will block execution.
    void SimulationCallbackHandler::onAdvance(const PxRigidBody *const *bodyBuffer,
        const PxTransform *poseBuffer,
        const PxU32 count) {
        Logf("SimulationCallbackHandler::onAdvance: %u", count);
    }

    static const auto collisionTable = [] {
        constexpr auto count = magic_enum::enum_count<ecs::PhysicsGroup>();
        std::array<std::array<PxPairFlags, count>, count> table;
        auto defaultFlags = PxPairFlag::eCONTACT_DEFAULT | PxPairFlag::ePRE_SOLVER_VELOCITY |
                            PxPairFlag::ePOST_SOLVER_VELOCITY | PxPairFlag::eNOTIFY_THRESHOLD_FORCE_FOUND |
                            PxPairFlag::eNOTIFY_THRESHOLD_FORCE_LOST | PxPairFlag::eNOTIFY_CONTACT_POINTS |
                            PxPairFlag::eNOTIFY_TOUCH_FOUND | PxPairFlag::eNOTIFY_TOUCH_LOST |
                            PxPairFlag::eNOTIFY_TOUCH_PERSISTS;
        std::fill(&table[0][0], &table[count - 1][count - 1] + 1, defaultFlags);

        auto removeCollision = [&](auto group0, auto group1) {
            table[(size_t)group0][(size_t)group1] = PxPairFlags();
            table[(size_t)group1][(size_t)group0] = PxPairFlags();
        };
        auto setPairFlags = [&](auto group0, auto group1, auto flags) {
            table[(size_t)group0][(size_t)group1] = flags;
            table[(size_t)group1][(size_t)group0] = flags;
        };

        for (auto &group : magic_enum::enum_values<ecs::PhysicsGroup>()) {
            // Don't collide anything with the noclip group.
            removeCollision(group, ecs::PhysicsGroup::NoClip);
            if (group == ecs::PhysicsGroup::NoClip) continue;

            if (group == ecs::PhysicsGroup::PlayerLeftHand || group == ecs::PhysicsGroup::PlayerRightHand) {
                // Track precise touch events on player hands
                setPairFlags(group,
                    ecs::PhysicsGroup::World,
                    defaultFlags | PxPairFlag::eNOTIFY_CONTACT_POINTS | PxPairFlag::eNOTIFY_TOUCH_FOUND |
                        PxPairFlag::eNOTIFY_TOUCH_LOST);
            } else {
                // Only collide the player's hands with the user interface group
                removeCollision(group, ecs::PhysicsGroup::UserInterface);
            }
        }

        // Don't collide the player with themselves, but allow the hands to collide with eachother
        removeCollision(ecs::PhysicsGroup::Player, ecs::PhysicsGroup::Player);
        removeCollision(ecs::PhysicsGroup::Player, ecs::PhysicsGroup::PlayerLeftHand);
        removeCollision(ecs::PhysicsGroup::Player, ecs::PhysicsGroup::PlayerRightHand);
        removeCollision(ecs::PhysicsGroup::PlayerLeftHand, ecs::PhysicsGroup::PlayerLeftHand);
        removeCollision(ecs::PhysicsGroup::PlayerRightHand, ecs::PhysicsGroup::PlayerRightHand);
        return table;
    }();

    PxFilterFlags SimulationCallbackHandler::SimulationFilterShader(PxFilterObjectAttributes attributes0,
        PxFilterData filterData0,
        PxFilterObjectAttributes attributes1,
        PxFilterData filterData1,
        PxPairFlags &pairFlags,
        const void *constantBlock,
        PxU32 constantBlockSize) {
        if (PxFilterObjectIsTrigger(attributes0) || PxFilterObjectIsTrigger(attributes1)) {
            pairFlags = PxPairFlag::eTRIGGER_DEFAULT;
            return PxFilterFlags();
        }

        pairFlags = collisionTable[filterData0.word0][filterData1.word0];
        if (pairFlags == PxPairFlags()) {
            return PxFilterFlag::eSUPPRESS;
        }
        return PxFilterFlags();
    }

} // namespace sp
