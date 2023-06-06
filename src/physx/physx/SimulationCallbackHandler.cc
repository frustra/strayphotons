/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "SimulationCallbackHandler.hh"

#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "input/BindingNames.hh"
#include "physx/PhysxManager.hh"

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
        for (size_t i = 0; i < nbPairs; i++) {
            auto &pair = pairs[i];
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

            if (pair.events.isSet(PxPairFlag::eNOTIFY_THRESHOLD_FORCE_FOUND)) {
                if (shapeUserData0->owner != shapeUserData0->parent) {
                    ecs::EventBindings::SendEvent(lock,
                        shapeUserData0->owner,
                        ecs::Event{PHYSICS_EVENT_COLLISION_FORCE_FOUND, shapeUserData1->parent, thresholdForce});
                }
                if (shapeUserData1->owner != shapeUserData1->parent) {
                    ecs::EventBindings::SendEvent(lock,
                        shapeUserData1->owner,
                        ecs::Event{PHYSICS_EVENT_COLLISION_FORCE_FOUND, shapeUserData0->parent, thresholdForce});
                }

                ecs::EventBindings::SendEvent(lock,
                    shapeUserData0->parent,
                    ecs::Event{PHYSICS_EVENT_COLLISION_FORCE_FOUND, shapeUserData1->parent, thresholdForce});
                ecs::EventBindings::SendEvent(lock,
                    shapeUserData1->parent,
                    ecs::Event{PHYSICS_EVENT_COLLISION_FORCE_FOUND, shapeUserData0->parent, thresholdForce});
            } else if (pair.events.isSet(PxPairFlag::eNOTIFY_THRESHOLD_FORCE_LOST)) {
                ecs::EventBindings::SendEvent(lock,
                    shapeUserData0->parent,
                    ecs::Event{PHYSICS_EVENT_COLLISION_FORCE_LOST, shapeUserData1->parent, thresholdForce});
                ecs::EventBindings::SendEvent(lock,
                    shapeUserData1->parent,
                    ecs::Event{PHYSICS_EVENT_COLLISION_FORCE_LOST, shapeUserData0->parent, thresholdForce});

                if (shapeUserData0->owner != shapeUserData0->parent) {
                    ecs::EventBindings::SendEvent(lock,
                        shapeUserData0->owner,
                        ecs::Event{PHYSICS_EVENT_COLLISION_FORCE_LOST, shapeUserData1->parent, thresholdForce});
                }
                if (shapeUserData1->owner != shapeUserData1->parent) {
                    ecs::EventBindings::SendEvent(lock,
                        shapeUserData1->owner,
                        ecs::Event{PHYSICS_EVENT_COLLISION_FORCE_LOST, shapeUserData0->parent, thresholdForce});
                }
            }
        }
    }

    // Trigger events require an actor to have the PxShapeFlag::eTRIGGER_SHAPE simulation flag.
    void SimulationCallbackHandler::onTrigger(PxTriggerPair *pairs, PxU32 count) {
        Logf("SimulationCallbackHandler::onTrigger: %u", count);
    }

    // Called for rigid bodies that have moved and have the PxRigidBodyFlag::eENABLE_POSE_INTEGRATION_PREVIEW flag set.
    // This callback is invoked inline with the simulation and will block execution.
    void SimulationCallbackHandler::onAdvance(const PxRigidBody *const *bodyBuffer,
        const PxTransform *poseBuffer,
        const PxU32 count) {
        Logf("SimulationCallbackHandler::onAdvance: %u", count);
    }

    static const auto collisionTable = [] {
        constexpr auto count = magic_enum::enum_count<ecs::PhysicsGroup>();
        std::array<std::array<PxPairFlags, count>, count> table;
        auto defaultFlags = PxPairFlag::eCONTACT_DEFAULT | PxPairFlag::eNOTIFY_THRESHOLD_FORCE_FOUND |
                            PxPairFlag::eNOTIFY_THRESHOLD_FORCE_LOST;
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
