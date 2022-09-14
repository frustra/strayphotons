#include "CharacterControlSystem.hh"

#include "console/CVar.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/Ecs.hh"
#include "ecs/EcsImpl.hh"
#include "game/GameEntities.hh"
#include "game/Scene.hh"
#include "game/SceneManager.hh"
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
    static CVar<float> CVarCharacterAirStrafe("p.CharacterAirStrafe",
        0.8,
        "Character controller air strafe multiplier");
    static CVar<float> CVarCharacterJumpHeight("p.CharacterJumpHeight",
        0.4,
        "Character controller gravity jump multiplier");
    static CVar<float> CVarCharacterFlipSpeed("p.CharacterFlipSpeed",
        10,
        "Character controller reorientation speed (degrees/s)");
    static CVar<float> CVarCharacterMinFlipGravity("p.CharacterMinFlipGravity",
        8.0,
        "Character controller minimum gravity required to orient (m/s^2)");

    CharacterControlSystem::CharacterControlSystem(PhysxManager &manager) : manager(manager) {
        GetSceneManager().QueueActionAndBlock(SceneAction::ApplySystemScene,
            "character",
            [this](ecs::Lock<ecs::AddRemove> lock, std::shared_ptr<Scene> scene) {
                auto ent = scene->NewSystemEntity(lock, scene, entities::Head.Name());
                auto &tree = ent.Set<ecs::TransformTree>(lock);
                tree.parent = entities::Flatview;
                auto &script = ent.Set<ecs::Script>(lock);
                script.AddOnTick(ecs::EntityScope{scene, ecs::Name(scene->name, "")},
                    [](ecs::ScriptState &state,
                        ecs::Lock<ecs::WriteAll> lock,
                        ecs::Entity ent,
                        chrono_clock::duration interval) {
                        if (!ent.Has<ecs::TransformTree>(lock)) return;
                        auto &tree = ent.Get<ecs::TransformTree>(lock);

                        ecs::Entity hmd = entities::VrHmd.Get(lock);
                        ecs::Entity flatview = entities::Flatview.Get(lock);
                        if (hmd.Has<ecs::TransformTree>(lock)) {
                            tree.parent = hmd;
                        } else {
                            tree.parent = flatview;
                        }
                    });
            });

        auto lock = ecs::StartTransaction<ecs::AddRemove>();
        characterControllerObserver = lock.Watch<ecs::ComponentEvent<ecs::CharacterController>>();
    }

    glm::vec3 getHeadPosition(const PxCapsuleController *pxController) {
        auto headOffset = PxVec3ToGlmVec3(pxController->getUpDirection()) * pxController->getHeight() * 0.5f;
        return PxExtendedVec3ToGlmVec3(pxController->getPosition()) + headOffset;
    }

    void setHeadPosition(PxCapsuleController *pxController, glm::vec3 position) {
        auto upVector = PxVec3ToGlmVec3(pxController->getUpDirection());
        auto capsulePosition = position - upVector * pxController->getHeight() * 0.5f;
        pxController->setPosition(GlmVec3ToPxExtendedVec3(capsulePosition));

        // Updating the controller position does not update the underlying actor, we need to do it ourselves.
        PxTransform globalPose(GlmVec3ToPxVec3(capsulePosition));
        globalPose.q = PxShortestRotation(PxVec3(1.0f, 0.0f, 0.0f), pxController->getUpDirection());
        pxController->getActor()->setGlobalPose(globalPose);
    }

    void setFootPosition(PxCapsuleController *pxController, glm::vec3 position) {
        auto upVector = PxVec3ToGlmVec3(pxController->getUpDirection());
        auto footOffset = pxController->getHeight() * 0.5f + pxController->getRadius() +
                          pxController->getContactOffset();
        auto capsulePosition = position + upVector * footOffset;
        pxController->setPosition(GlmVec3ToPxExtendedVec3(capsulePosition));

        // Updating the controller position does not update the underlying actor, we need to do it ourselves.
        PxTransform globalPose(GlmVec3ToPxVec3(capsulePosition));
        globalPose.q = PxShortestRotation(PxVec3(1.0f, 0.0f, 0.0f), pxController->getUpDirection());
        pxController->getActor()->setGlobalPose(globalPose);
    }

    void CharacterControlSystem::Frame(ecs::Lock<ecs::ReadSignalsLock,
        ecs::Read<ecs::EventInput, ecs::SceneInfo>,
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

                        desc.climbingMode = PxCapsuleClimbingMode::eCONSTRAINED;
                        desc.nonWalkableMode = PxControllerNonWalkableMode::ePREVENT_CLIMBING_AND_FORCE_SLIDING;
                        desc.slopeLimit = cos(glm::radians(30.0f));

                        desc.material = characterUserData->actorData.material.get();
                        desc.userData = characterUserData;

                        // Offset capsule position so the feet are the origin
                        desc.position.y += desc.contactOffset + desc.radius + desc.height * 0.5f;

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

            ecs::SceneProperties sceneProperties = {};
            if (entity.Has<ecs::SceneInfo>(lock)) {
                auto &properties = entity.Get<ecs::SceneInfo>(lock).properties;
                if (properties) sceneProperties = *properties;
            }

            auto actor = controller.pxController->getActor();
            auto userData = (CharacterControllerUserData *)controller.pxController->getUserData();
            float contactOffset = controller.pxController->getContactOffset();
            float capsuleRadius = controller.pxController->getRadius();

            float targetHeight = controller.pxController->getHeight();
            glm::vec3 deltaHeadPos(0);

            auto head = controller.head.Get(lock);
            if (head.Has<ecs::TransformTree>(lock)) {
                auto &headTree = head.Get<const ecs::TransformTree>(lock);
                auto headRoot = ecs::TransformTree::GetRoot(lock, head);
                auto relativeTransform = headTree.GetRelativeTransform(lock, headRoot);

                if (headTree.parent != userData->headTarget) {
                    // Move the head to the physics actor when the head changes
                    relativeTransform.matrix[0] = relativeTransform.GetRotation() * glm::vec3(1, 0, 0);
                    relativeTransform.matrix[1] = transform.GetUp();
                    relativeTransform.matrix[2] = glm::cross(relativeTransform.matrix[0], relativeTransform.matrix[1]);

                    auto targetRotation = glm::inverse(relativeTransform.GetRotation()) * transform.GetRotation();
                    auto headPosition = relativeTransform.GetPosition();
                    headPosition.y = 0;
                    auto targetPosition = transform.GetPosition() + targetRotation * -deltaHeadPos;

                    auto &rootTree = headRoot.Get<ecs::TransformTree>(lock);
                    rootTree.pose.SetRotation(targetRotation);
                    rootTree.pose.SetPosition(targetPosition);

                    userData->headTarget = headTree.parent.Get(lock);
                } else {
                    // Use head as a height target and directional movement input
                    deltaHeadPos = relativeTransform.GetPosition();
                    deltaHeadPos.y = 0;
                }

                auto playerHeight = relativeTransform.GetPosition().y;
                targetHeight = std::max(0.1f, playerHeight - capsuleRadius - contactOffset);
            }

            // If the entity moved, teleport the controller
            if (transform != userData->actorData.pose) {
                // Move the head to the new player position
                if (head.Has<ecs::TransformTree>(lock)) {
                    auto headRoot = ecs::TransformTree::GetRoot(lock, head);
                    auto &headTree = head.Get<const ecs::TransformTree>(lock);

                    auto relativeTransform = headTree.GetRelativeTransform(lock, headRoot);
                    relativeTransform.matrix[0] = relativeTransform.GetRotation() * glm::vec3(1, 0, 0);
                    relativeTransform.matrix[1] = transform.GetUp();
                    relativeTransform.matrix[2] = glm::cross(relativeTransform.matrix[0], relativeTransform.matrix[1]);

                    auto targetRotation = glm::inverse(relativeTransform.GetRotation()) * transform.GetRotation();
                    auto targetPosition = transform.GetPosition() + targetRotation * -deltaHeadPos;

                    auto &rootTree = headRoot.Get<ecs::TransformTree>(lock);
                    rootTree.pose.SetRotation(targetRotation);
                    rootTree.pose.SetPosition(targetPosition);
                }

                controller.pxController->setHeight(targetHeight);
                controller.pxController->setUpDirection(GlmVec3ToPxVec3(transform.GetUp()));
                setFootPosition(controller.pxController, transform.GetPosition());

                userData->onGround = false;
                userData->actorData.pose = transform;
                userData->actorData.velocity = glm::vec3(0);
            }

            bool noclip = ecs::SignalBindings::GetSignal(lock, entity, INPUT_SIGNAL_MOVE_NOCLIP) >= 0.5;

            // Update the capsule height
            auto currentHeight = controller.pxController->getHeight();
            if (currentHeight != targetHeight) {
                if (!noclip && targetHeight > currentHeight) {
                    // Check to see if there is room to expand the capsule
                    PxSweepBuffer hit;
                    PxCapsuleGeometry capsuleGeometry(capsuleRadius, currentHeight * 0.5f);
                    auto sweepDist = targetHeight - currentHeight + contactOffset;
                    bool status = manager.scene->sweep(capsuleGeometry,
                        actor->getGlobalPose(),
                        controller.pxController->getUpDirection(),
                        sweepDist,
                        hit,
                        PxHitFlags(),
                        PxQueryFilterData(filterData, PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC));
                    if (status) {
                        auto headroom = std::max(hit.block.distance - contactOffset, 0.0f);
                        currentHeight += headroom;
                    } else {
                        currentHeight = targetHeight;
                    }
                } else {
                    currentHeight = targetHeight;
                }

                auto footPosition = PxExtendedVec3ToGlmVec3(controller.pxController->getFootPosition());
                controller.pxController->setHeight(currentHeight);
                setFootPosition(controller.pxController, footPosition);
            }

            // Update the capsule orientation
            glm::vec3 gravityForce = sceneProperties.fixedGravity;
            if (sceneProperties.gravityFunction) {
                gravityForce = sceneProperties.gravityFunction(transform.GetPosition());
            }
            auto gravityStrength = glm::length(gravityForce);
            if (gravityStrength > 0 && gravityStrength > CVarCharacterMinFlipGravity.Get()) {
                auto targetUpVector = glm::normalize(-gravityForce);
                auto deltaRotation = glm::rotation(transform.GetUp(), targetUpVector);
                auto angleDiff = glm::angle(deltaRotation);
                auto rotationAxis = glm::axis(deltaRotation);

                angleDiff = std::min(angleDiff, glm::radians(CVarCharacterFlipSpeed.Get() * dt));
                deltaRotation = glm::angleAxis(angleDiff, rotationAxis);
                targetUpVector = deltaRotation * transform.GetUp();

                bool shouldRotate = angleDiff > 0;
                if (shouldRotate && !noclip) {
                    auto halfHeight = controller.pxController->getHeight() * 0.5f;
                    auto currentOffset = transform.GetUp() * halfHeight;
                    auto newOffset = targetUpVector * halfHeight;

                    PxOverlapHit touch;
                    PxOverlapBuffer overlapHit;
                    overlapHit.touches = &touch;
                    overlapHit.maxNbTouches = 1;
                    PxCapsuleGeometry capsuleGeometry(capsuleRadius, currentHeight * 0.5f);
                    auto globalPose = actor->getGlobalPose();
                    globalPose.p += GlmVec3ToPxVec3(currentOffset - newOffset);
                    globalPose.q = PxShortestRotation(PxVec3(1.0f, 0.0f, 0.0f), GlmVec3ToPxVec3(targetUpVector));
                    shouldRotate = !manager.scene->overlap(capsuleGeometry,
                        globalPose,
                        overlapHit,
                        PxQueryFilterData(filterData, PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC));
                }

                // Only rotate the capsule in there is room to do so
                if (shouldRotate) {
                    auto headPosition = getHeadPosition(controller.pxController);
                    controller.pxController->setUpDirection(GlmVec3ToPxVec3(targetUpVector));
                    setHeadPosition(controller.pxController, headPosition);

                    transform.SetPosition(PxExtendedVec3ToGlmVec3(controller.pxController->getFootPosition()));
                    transform.Rotate(deltaRotation);

                    // Rotate the head to match
                    if (head.Has<ecs::TransformTree>(lock)) {
                        auto &headTree = head.Get<const ecs::TransformTree>(lock);
                        auto oldPosition = headTree.GetGlobalTransform(lock).GetPosition();
                        auto headRoot = ecs::TransformTree::GetRoot(lock, head);
                        auto &rootTree = headRoot.Get<ecs::TransformTree>(lock);
                        rootTree.pose.Rotate(deltaRotation);
                        auto newPosition = headTree.GetGlobalTransform(lock).GetPosition();
                        rootTree.pose.Translate(oldPosition - newPosition);
                    }
                }
            }

            // Read character movement inputs
            glm::vec3 movementInput = glm::vec3(0);
            movementInput.x = ecs::SignalBindings::GetSignal(lock, entity, INPUT_SIGNAL_MOVE_RELATIVE_X);
            movementInput.y = ecs::SignalBindings::GetSignal(lock, entity, INPUT_SIGNAL_MOVE_RELATIVE_Y);
            movementInput.z = ecs::SignalBindings::GetSignal(lock, entity, INPUT_SIGNAL_MOVE_RELATIVE_Z);
            bool sprint = ecs::SignalBindings::GetSignal(lock, entity, INPUT_SIGNAL_MOVE_SPRINT) >= 0.5;

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

            if (userData->noclipping != noclip) {
                manager.SetCollisionGroup(actor, noclip ? ecs::PhysicsGroup::NoClip : ecs::PhysicsGroup::Player);
                userData->noclipping = noclip;
                controller.pxController->invalidateCache();
            }

            auto headInput = glm::clamp(deltaHeadPos, -1.0f, 1.0f) * speed;

            // Update the capsule position, velocity, and onGround flag
            if (noclip) {
                auto worldVelocity = transform.GetRotation() * (movementInput + headInput);
                transform.Translate(worldVelocity * dt);
                setFootPosition(controller.pxController, transform.GetPosition());

                // Move the head to the new player position
                if (head.Has<ecs::TransformTree>(lock)) {
                    auto headRoot = ecs::TransformTree::GetRoot(lock, head);
                    auto &rootTree = headRoot.Get<ecs::TransformTree>(lock);
                    rootTree.pose.Translate(worldVelocity * dt);
                }

                userData->onGround = false;
                userData->actorData.velocity = worldVelocity;
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
                PxCapsuleGeometry capsuleGeometry(capsuleRadius, currentHeight * 0.5f);
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
                    userData->actorData.velocity += worldMovement * CVarCharacterAirStrafe.Get() * dt;
                    displacement = userData->actorData.velocity * dt;
                }
                auto headDisplacement = transform.GetRotation() * headInput * dt;
                // Logf("Disp: %s + %s, State:%u, On:%u, In:%u, DeltaXp: %s, Vel: %s",
                //     glm::to_string(displacement),
                //     glm::to_string(targetDelta),
                //     state.collisionFlags & PxControllerCollisionFlag::eCOLLISION_DOWN,
                //     userData->onGround,
                //     inGround,
                //     glm::to_string(PxVec3ToGlmVec3(state.deltaXP)),
                //     glm::to_string(userData->actorData.velocity));

                auto oldPosition = PxExtendedVec3ToGlmVec3(controller.pxController->getFootPosition());

                auto moveResult = controller.pxController->move(GlmVec3ToPxVec3(displacement + headDisplacement),
                    0,
                    dt,
                    moveQueryFilter);

                controller.pxController->getState(state);

                auto newPosition = PxExtendedVec3ToGlmVec3(controller.pxController->getFootPosition());
                auto deltaPos = newPosition - oldPosition - headDisplacement;

                PxSweepBuffer sweepHit;
                auto sweepStart = actor->getGlobalPose();
                sweepStart.p = physx::toVec3(controller.pxController->getPosition());
                sweepStart.p += controller.pxController->getUpDirection() * contactOffset;
                bool onGround = manager.scene->sweep(capsuleGeometry,
                    sweepStart,
                    -controller.pxController->getUpDirection(),
                    contactOffset,
                    sweepHit,
                    PxHitFlag::ePOSITION,
                    PxQueryFilterData(filterData, PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC));

                if (state.touchedActor) {
                    auto touchedUserData = (ActorUserData *)state.touchedActor->userData;
                    if (touchedUserData) userData->standingOn = touchedUserData->entity;
                }

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
                        if (jump) userData->actorData.velocity -= gravityForce * CVarCharacterJumpHeight.Get();
                        // Logf("WasOn: %u, In: %u, Jump: %u, DeltaXp: %s, Vel: %s",
                        //     userData->onGround,
                        //     inGround,
                        //     jump,
                        //     glm::to_string(PxVec3ToGlmVec3(state.deltaXP)),
                        //     glm::to_string(userData->actorData.velocity));
                    } else {
                        userData->actorData.velocity = deltaPos / dt;
                        userData->actorData.velocity += gravityForce * dt;
                        // Logf("OffGround, DeltaPos: %s - %s, Vel: %s",
                        //     glm::to_string(newPosition - userData->actorData.pose.GetPosition()),
                        //     glm::to_string(targetDelta),
                        //     glm::to_string(userData->actorData.velocity));
                    }

                    userData->onGround = false;
                }

                // Move the head to the new player position
                if (head.Has<ecs::TransformTree>(lock)) {
                    auto headRoot = ecs::TransformTree::GetRoot(lock, head);
                    auto &rootTree = headRoot.Get<ecs::TransformTree>(lock);

                    // Only move the head if the movement is in line with the input displacement
                    // This allows the head to detatch from the player when colliding with walls
                    auto verticalDeltaPos = transform.GetUp() * glm::dot(transform.GetUp(), deltaPos);
                    auto flatDisplacement = displacement -
                                            transform.GetUp() * glm::dot(transform.GetUp(), displacement);
                    auto modifiedDeltaPos = glm::vec3(0);
                    if (flatDisplacement != glm::vec3(0)) {
                        auto displacementDir = glm::normalize(flatDisplacement);
                        modifiedDeltaPos = displacementDir * std::max(0.0f, glm::dot(displacementDir, deltaPos));
                    }
                    modifiedDeltaPos += verticalDeltaPos;
                    rootTree.pose.Translate(modifiedDeltaPos);

                    if (headRoot.Has<ecs::Physics>(lock) && manager.actors.count(headRoot) != 0) {
                        auto &proxyActor = manager.actors[headRoot];
                        if (proxyActor && proxyActor->userData) {
                            auto proxyUserData = (ActorUserData *)proxyActor->userData;
                            proxyUserData->velocity = userData->actorData.velocity;
                        }
                    }
                }

                transform.SetPosition(newPosition);
            }

            userData->actorData.pose = transform;
        }
    }
} // namespace sp
