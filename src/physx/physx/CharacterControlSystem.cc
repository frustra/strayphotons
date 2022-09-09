#include "CharacterControlSystem.hh"

#include "console/CVar.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/Ecs.hh"
#include "ecs/EcsImpl.hh"
#include "input/BindingNames.hh"
#include "physx/PhysxManager.hh"
#include "physx/PhysxUtils.hh"

#include <PxRigidActor.h>
#include <PxScene.h>
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtx/vector_angle.hpp>
#include <sstream>

namespace sp {
    using namespace physx;

    static CVar<float> CVarCharacterMovementSpeed("p.CharacterMovementSpeed",
        3.0,
        "Character controller movement speed (m/s)");
    static CVar<float> CVarCharacterSprintSpeed("p.CharacterSprintSpeed",
        5.0,
        "Character controller sprint speed (m/s)");
    static CVar<float> CVarCharacterFlipSpeed("p.CharacterFlipSpeed",
        0.25,
        "Character controller reorientation speed (gravity dependant)");
    static CVar<float> CVarCharacterMinFlipGravity("p.CharacterMinFlipGravity",
        8.0,
        "Character controller minimum gravity required to orient (m/s^2)");

    CharacterControlSystem::CharacterControlSystem(PhysxManager &manager) : manager(manager) {
        auto lock = ecs::StartTransaction<ecs::AddRemove>();
        characterControllerObserver = lock.Watch<ecs::ComponentEvent<ecs::CharacterController>>();
    }

    void CharacterControlSystem::Frame(ecs::Lock<ecs::ReadSignalsLock,
        ecs::Read<ecs::EventInput>,
        ecs::Write<ecs::TransformTree, ecs::CharacterController>> lock) {
        // Update PhysX with any added or removed CharacterControllers
        ecs::ComponentEvent<ecs::CharacterController> controllerEvent;
        while (characterControllerObserver.Poll(lock, controllerEvent)) {
            if (controllerEvent.type == Tecs::EventType::ADDED) {
                if (controllerEvent.entity.Has<ecs::CharacterController>(lock)) {
                    auto &controller = controllerEvent.entity.Get<ecs::CharacterController>(lock);
                    if (!controller.pxController) {
                        auto characterUserData = new CharacterControllerUserData(controllerEvent.entity);

                        characterUserData->actorData.material = std::shared_ptr<PxMaterial>(
                            manager.pxPhysics->createMaterial(0.3f, 0.3f, 0.3f),
                            [](auto *ptr) {
                                ptr->release();
                            });

                        PxCapsuleControllerDesc desc;
                        desc.position = PxExtendedVec3(0, 0, 0);
                        desc.upDirection = PxVec3(0, 1, 0);
                        desc.radius = ecs::PLAYER_RADIUS;
                        desc.height = ecs::PLAYER_CAPSULE_HEIGHT;
                        desc.stepOffset = ecs::PLAYER_STEP_HEIGHT;
                        desc.scaleCoeff = 1.0f; // Why is the default 0.8? No idea...
                        // Decreasing the contactOffset value causes the player to be able to stand on
                        // very thin (and likely unintentional) ledges.
                        desc.contactOffset = 0.05f;
                        desc.material = characterUserData->actorData.material.get();
                        desc.climbingMode = PxCapsuleClimbingMode::eCONSTRAINED;
                        desc.userData = characterUserData;

                        auto pxController = manager.controllerManager->createController(desc);
                        Assert(pxController->getType() == PxControllerShapeType::eCAPSULE,
                            "Physx did not create a valid PxCapsuleController");

                        auto actor = pxController->getActor();
                        actor->userData = &characterUserData->actorData;

                        manager.SetCollisionGroup(actor, ecs::PhysicsGroup::Player);

                        controller.pxController = static_cast<PxCapsuleController *>(pxController);
                    }
                }
            } else if (controllerEvent.type == Tecs::EventType::REMOVED) {
                if (controllerEvent.component.pxController) {
                    auto userData = controllerEvent.component.pxController->getUserData();
                    if (userData) {
                        delete (CharacterControllerUserData *)userData;
                        controllerEvent.component.pxController->setUserData(nullptr);
                    }

                    controllerEvent.component.pxController->release();
                }
            }
        }

        PxFilterData filterData;
        filterData.word0 = ecs::PHYSICS_GROUP_WORLD | ecs::PHYSICS_GROUP_WORLD_OVERLAP | ecs::PHYSICS_GROUP_INTERACTIVE;
        PxControllerFilters moveQueryFilter(&filterData);

        float dt = (float)(manager.interval.count() / 1e9);

        for (auto &entity : lock.EntitiesWith<ecs::CharacterController>()) {
            if (!entity.Has<ecs::CharacterController, ecs::TransformTree>(lock)) continue;

            auto &controller = entity.Get<ecs::CharacterController>(lock);
            if (!controller.pxController) continue;
            auto &transformTree = entity.Get<ecs::TransformTree>(lock);
            Assertf(!transformTree.parent,
                "CharacterController should not have a TransformTree parent: %s",
                transformTree.parent.Name().String());
            auto &transform = transformTree.pose;

            auto actor = controller.pxController->getActor();
            auto userData = (CharacterControllerUserData *)controller.pxController->getUserData();
            float contactOffset = controller.pxController->getContactOffset();

            float targetHeight = ecs::PLAYER_CAPSULE_HEIGHT;
            glm::vec3 targetPosition = transform.GetPosition();

            auto target = controller.target.Get(lock);
            if (!target.Has<ecs::TransformTree>(lock) || !target.Get<const ecs::TransformTree>(lock).parent) {
                target = controller.fallbackTarget.Get(lock);
            }
            if (target.Has<ecs::TransformTree>(lock)) {
                auto &targetTree = target.Get<const ecs::TransformTree>(lock);
                targetPosition = targetTree.GetGlobalTransform(lock).GetPosition();
                auto playerHeight = glm::dot(transform.GetUp(), targetPosition - transform.GetPosition());
                targetPosition -= playerHeight * transform.GetUp();
                targetHeight = std::max(0.1f, playerHeight - ecs::PLAYER_RADIUS);

                if (target != userData->target) {
                    // Move the target to the physics actor when the target changes
                    auto movementProxy = controller.movementProxy.Get(lock);
                    if (movementProxy.Has<ecs::TransformTree>(lock)) {
                        auto &proxyTransform = movementProxy.Get<ecs::TransformTree>(lock);
                        auto deltaPos = userData->actorData.pose.GetPosition() - targetPosition;
                        proxyTransform.pose.Translate(deltaPos);
                        targetPosition += deltaPos;
                    }

                    userData->target = target;
                }
            }

            // If the entity moved, teleport the controller
            if (transform.GetPosition() != userData->actorData.pose.GetPosition()) {
                controller.pxController->setHeight(targetHeight);
                controller.pxController->setFootPosition(GlmVec3ToPxExtendedVec3(targetPosition));

                // Updating the controller position does not update the underlying actor, we need to do it ourselves.
                auto capsulePosition = controller.pxController->getPosition();
                auto actorPose = actor->getGlobalPose();
                actorPose.p = PxVec3(capsulePosition.x, capsulePosition.y, capsulePosition.z);
                actor->setGlobalPose(actorPose);

                userData->onGround = false;
                userData->actorData.pose.SetPosition(targetPosition);
                userData->actorData.velocity = glm::vec3(0);
            }

            // Update the capsule height
            auto currentHeight = controller.pxController->getHeight();
            if (currentHeight != targetHeight) {
                if (targetHeight > currentHeight) {
                    // Check to see if there is room to expand the capsule
                    PxSweepBuffer hit;
                    PxCapsuleGeometry capsuleGeometry(ecs::PLAYER_RADIUS, currentHeight * 0.5f);
                    auto sweepDist = targetHeight - currentHeight + contactOffset;
                    bool status = manager.scene->sweep(capsuleGeometry,
                        actor->getGlobalPose(),
                        PxVec3(0, 1, 0),
                        sweepDist,
                        hit,
                        PxHitFlags(),
                        PxQueryFilterData(filterData, PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC));
                    if (status) {
                        auto headroom = std::max(hit.block.distance - contactOffset, 0.0f);
                        controller.pxController->resize(currentHeight + headroom);
                    } else {
                        controller.pxController->resize(targetHeight);
                    }
                } else {
                    controller.pxController->resize(targetHeight);
                }

                // Updating the controller position does not update the underlying actor, we need to do it ourselves.
                auto capsulePosition = controller.pxController->getPosition();
                auto actorPose = actor->getGlobalPose();
                actorPose.p = PxVec3(capsulePosition.x, capsulePosition.y, capsulePosition.z);
                actor->setGlobalPose(actorPose);
                currentHeight = controller.pxController->getHeight();
            }

            // Read character movement inputs
            glm::vec3 movementInput = glm::vec3(0);
            movementInput.x = ecs::SignalBindings::GetSignal(lock, entity, INPUT_SIGNAL_MOVE_RELATIVE_X);
            movementInput.y = ecs::SignalBindings::GetSignal(lock, entity, INPUT_SIGNAL_MOVE_RELATIVE_Y);
            movementInput.z = ecs::SignalBindings::GetSignal(lock, entity, INPUT_SIGNAL_MOVE_RELATIVE_Z);
            bool sprint = ecs::SignalBindings::GetSignal(lock, entity, INPUT_SIGNAL_MOVE_SPRINT) >= 0.5;
            bool noclip = ecs::SignalBindings::GetSignal(lock, entity, INPUT_SIGNAL_MOVE_NOCLIP) >= 0.5;

            bool jump = false;
            if (entity.Has<ecs::EventInput>(lock)) {
                ecs::Event event;
                while (ecs::EventInput::Poll(lock, entity, "/action/jump", event)) {
                    jump = true;
                }
            }

            float speed = sprint ? CVarCharacterSprintSpeed.Get() : CVarCharacterMovementSpeed.Get();
            if (movementInput.x != 0 || movementInput.z != 0) {
                auto normalized = glm::normalize(glm::vec3(movementInput.x, 0, movementInput.z)) * speed;
                movementInput.x = normalized.x;
                movementInput.z = normalized.z;
            }
            movementInput.y = std::clamp(movementInput.y, -1.0f, 1.0f) * speed;

            auto targetDelta = targetPosition - userData->actorData.pose.GetPosition();

            if (userData->noclipping != noclip) {
                manager.SetCollisionGroup(actor, noclip ? ecs::PhysicsGroup::NoClip : ecs::PhysicsGroup::Player);
                userData->noclipping = noclip;
                controller.pxController->invalidateCache();
            }

            // Update the capsule position, velocity, and onGround flag
            if (noclip) {
                auto worldMovement = transform.GetRotation() * movementInput;
                auto deltaPos = worldMovement * dt + targetDelta;
                targetPosition += deltaPos;
                controller.pxController->setFootPosition(GlmVec3ToPxExtendedVec3(targetPosition));
                transform.SetPosition(targetPosition);

                auto movementProxy = controller.movementProxy.Get(lock);
                if (movementProxy.Has<ecs::TransformTree>(lock)) {
                    auto &proxyTransform = movementProxy.Get<ecs::TransformTree>(lock);
                    proxyTransform.pose.Translate(deltaPos);
                }

                userData->onGround = false;
                userData->actorData.velocity = worldMovement;
            } else {
                PxControllerState state;
                controller.pxController->getState(state);

                // If player is moving up, onGround detection does not work, so we need to do it ourselves.
                // This edge-case is possible when jumping in an elevator; the floor can catch up to the player
                // while their velocity is still in the up direction.
                PxOverlapHit touch;
                PxOverlapBuffer overlapHit;
                overlapHit.touches = &touch;
                overlapHit.maxNbTouches = 1;
                PxCapsuleGeometry capsuleGeometry(ecs::PLAYER_RADIUS, currentHeight * 0.5f);
                bool inGround = manager.scene->overlap(capsuleGeometry,
                    actor->getGlobalPose(),
                    overlapHit,
                    PxQueryFilterData(filterData, PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC));

                glm::vec3 displacement;
                if (userData->onGround || inGround) {
                    auto relativeFloorVelocity = glm::inverse(transform.GetRotation()) * PxVec3ToGlmVec3(state.deltaXP);
                    displacement = (relativeFloorVelocity + glm::vec3(movementInput.x, 0, movementInput.z)) * dt;
                    if (jump) {
                        // Move up slightly first to detach the player from the floor
                        displacement.y = std::max(relativeFloorVelocity.y * dt, 0.0f) + contactOffset;
                    } else {
                        // Always move down slightly for consistent onGround detection
                        displacement.y = -contactOffset;
                    }
                    displacement = transform.GetRotation() * displacement;
                } else {
                    auto worldMovement = transform.GetRotation() * movementInput;
                    userData->actorData.velocity += worldMovement * ecs::PLAYER_AIR_STRAFE * dt;
                    displacement = userData->actorData.velocity * dt;
                }
                // Logf("Disp: %s + %s, State:%u, On:%u, In:%u, DeltaXp: %s, Vel: %s",
                //     glm::to_string(displacement),
                //     glm::to_string(targetDelta),
                //     state.collisionFlags & PxControllerCollisionFlag::eCOLLISION_DOWN,
                //     userData->onGround,
                //     inGround,
                //     glm::to_string(PxVec3ToGlmVec3(state.deltaXP)),
                //     glm::to_string(userData->actorData.velocity));

                auto footPos = controller.pxController->getFootPosition();
                controller.pxController->setUpDirection(GlmVec3ToPxVec3(transform.GetUp()));
                controller.pxController->setFootPosition(footPos);

                auto moveResult =
                    controller.pxController->move(GlmVec3ToPxVec3(displacement + targetDelta), 0, dt, moveQueryFilter);

                controller.pxController->getState(state);

                auto newPosition = PxExtendedVec3ToGlmVec3(controller.pxController->getFootPosition());
                auto deltaPos = newPosition - userData->actorData.pose.GetPosition() - targetDelta;

                PxSweepBuffer sweepHit;
                auto sweepStart = actor->getGlobalPose();
                sweepStart.p = physx::toVec3(controller.pxController->getPosition());
                bool onGround = manager.scene->sweep(capsuleGeometry,
                    sweepStart,
                    -controller.pxController->getUpDirection(),
                    controller.pxController->getContactOffset(),
                    sweepHit,
                    PxHitFlag::ePOSITION,
                    PxQueryFilterData(filterData, PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC));

                if (state.touchedActor) {
                    auto touchedUserData = (ActorUserData *)state.touchedActor->userData;
                    if (touchedUserData) userData->standingOn = touchedUserData->entity;
                }

                auto gravityFunction = [](glm::vec3 position) {
                    position.x = 0;
                    // Derived from centripetal acceleration formula, rotating around the origin
                    float spinTerm = M_PI * CVarGravitySpin.Get() / 30;
                    return spinTerm * spinTerm * position;
                };

                if (moveResult & PxControllerCollisionFlag::eCOLLISION_DOWN || onGround) {
                    userData->actorData.velocity = PxVec3ToGlmVec3(state.deltaXP);
                    userData->onGround = true;
                    // Logf("OnGround, Vel: %s", glm::to_string(userData->actorData.velocity));
                } else {
                    if (userData->onGround || inGround) {
                        // Remove any vertical component from the displacement before adding to velocity
                        auto flatDisplacement = displacement -
                                                transform.GetUp() * glm::dot(transform.GetUp(), displacement);
                        userData->actorData.velocity = PxVec3ToGlmVec3(state.deltaXP);
                        userData->actorData.velocity += flatDisplacement / dt;
                        if (jump)
                            userData->actorData.velocity -= gravityFunction(newPosition) *
                                                            0.5f; // ecs::PLAYER_JUMP_VELOCITY;
                        // Logf("WasOn: %u, In: %u, Jump: %u, DeltaXp: %s, Vel: %s",
                        //     userData->onGround,
                        //     inGround,
                        //     jump,
                        //     glm::to_string(PxVec3ToGlmVec3(state.deltaXP)),
                        //     glm::to_string(userData->actorData.velocity));
                    } else {
                        userData->actorData.velocity = deltaPos / dt;
                        userData->actorData.velocity += gravityFunction(newPosition) * dt;
                        // userData->actorData.velocity.y -= ecs::PLAYER_GRAVITY * dt;
                        // Logf("OffGround, DeltaPos: %s - %s, Vel: %s",
                        //     glm::to_string(newPosition - userData->actorData.pose.GetPosition()),
                        //     glm::to_string(targetDelta),
                        //     glm::to_string(userData->actorData.velocity));
                    }

                    userData->onGround = false;
                }

                auto movementProxy = controller.movementProxy.Get(lock);
                if (movementProxy.Has<ecs::TransformTree>(lock)) {
                    auto &proxyTransform = movementProxy.Get<ecs::TransformTree>(lock);
                    // Only move the VR player if the movement is in line with the input displacement
                    // This allows the headset to detatch from the player capsule so they don't get pushed back by walls
                    auto absDeltaPos = glm::abs(deltaPos) + 0.00001f;
                    glm::vec3 clampRatio = glm::min(glm::abs(displacement), absDeltaPos) / absDeltaPos;
                    if (glm::sign(deltaPos.x) != glm::sign(displacement.x)) clampRatio.x = 0.0f;
                    clampRatio.y = 1.0f;
                    if (glm::sign(deltaPos.z) != glm::sign(displacement.z)) clampRatio.z = 0.0f;
                    proxyTransform.pose.Translate(deltaPos * clampRatio);

                    if (movementProxy.Has<ecs::Physics>(lock) && manager.actors.count(movementProxy) != 0) {
                        auto &proxyActor = manager.actors[movementProxy];
                        if (proxyActor && proxyActor->userData) {
                            auto proxyUserData = (ActorUserData *)proxyActor->userData;
                            proxyUserData->velocity = userData->actorData.velocity;
                        }
                    }
                }

                transform.SetPosition(newPosition);
                auto gravityVector = gravityFunction(newPosition);
                auto gravityStrength = glm::length(gravityVector);
                if (gravityStrength > CVarCharacterMinFlipGravity.Get()) {
                    auto targetUpVector = glm::normalize(-gravityVector);
                    auto targetRotation = glm::rotation(glm::vec3(0, 1, 0), targetUpVector);

                    float factor = glm::radians(gravityStrength * CVarCharacterFlipSpeed.Get() * dt) /
                                   glm::angle(transform.GetUp(), targetUpVector);
                    factor = glm::min(factor, 1.0f);
                    transform.SetRotation(glm::shortMix(transform.GetRotation(), targetRotation, factor));
                }
            }

            userData->actorData.pose = transform;
        }
    }
} // namespace sp
