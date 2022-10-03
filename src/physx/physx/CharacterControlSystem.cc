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
    static CVar<float> CVarCharacterMaxHeadSpeed("p.CharacterMaxHeadSpeed",
        5.0,
        "Character controller max head movement speed (m/s)");
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
                        if (hmd.Has<ecs::TransformTree>(lock) && hmd.Get<ecs::TransformTree>(lock).parent) {
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

            auto head = controller.head.Get(lock);
            if (!head.Has<ecs::TransformTree>(lock)) continue;

            ecs::SceneProperties sceneProperties = {};
            if (entity.Has<ecs::SceneInfo>(lock)) {
                auto &properties = entity.Get<ecs::SceneInfo>(lock).properties;
                if (properties) sceneProperties = *properties;
            }

            auto actor = controller.pxController->getActor();
            auto userData = (CharacterControllerUserData *)controller.pxController->getUserData();
            float contactOffset = controller.pxController->getContactOffset();
            float capsuleRadius = controller.pxController->getRadius();

            auto headRoot = ecs::TransformTree::GetRoot(lock, head);
            auto &headTree = head.Get<const ecs::TransformTree>(lock);
            auto &rootTree = headRoot.Get<ecs::TransformTree>(lock);
            auto headRelativeRoot = headTree.GetRelativeTransform(lock, headRoot);

            auto playerHeight = headRelativeRoot.GetPosition().y;
            float targetHeight = std::max(0.1f, playerHeight - capsuleRadius - contactOffset);

            ecs::Transform headRelativePlayer;

            // If the entity moved or the head was retargeted, teleport the controller
            if (transform != userData->actorData.pose || headTree.parent != userData->headTarget) {
                controller.pxController->setHeight(targetHeight);
                controller.pxController->setUpDirection(GlmVec3ToPxVec3(transform.GetUp()));
                setFootPosition(controller.pxController, transform.GetPosition());

                // Move the head to the new player position and ensure it is facing forward
                auto forwardRelativeRoot = headRelativeRoot.GetForward();
                if (std::abs(forwardRelativeRoot.y) > 0.999) {
                    forwardRelativeRoot = headRelativeRoot.GetRotation() * glm::vec3(0, -forwardRelativeRoot.y, 0);
                }
                forwardRelativeRoot.y = 0;
                auto deltaRotation = glm::rotation(glm::normalize(forwardRelativeRoot), glm::vec3(0, 0, -1));
                headRelativePlayer.SetRotation(deltaRotation * headRelativeRoot.GetRotation());
                headRelativePlayer.SetPosition(glm::vec3(0, headRelativeRoot.GetPosition().y, 0));

                auto targetTransform = transform * headRelativePlayer;
                if (ecs::TransformTree::GetRoot(lock, head) != entity) {
                    ecs::TransformTree::MoveViaRoot(lock, head, targetTransform);
                }

                Logf("Teleport: %s, Up: %s, Height: %f Forward: %s",
                    glm::to_string(targetTransform.GetPosition()),
                    glm::to_string(targetTransform.GetUp()),
                    targetHeight,
                    glm::to_string(targetTransform.GetForward()));

                userData->onGround = false;
                userData->actorData.pose = transform;
                userData->actorData.velocity = glm::vec3(0);
                userData->headTarget = headTree.parent.Get(lock);
            } else {
                headRelativePlayer = headTree.GetRelativeTransform(lock, entity);
            }
            // Logf("Start headRelativePlayer pos: %s", glm::to_string(headRelativePlayer.GetPosition()));

            bool noclip = ecs::SignalBindings::GetSignal(lock, entity, INPUT_SIGNAL_MOVE_NOCLIP) >= 0.5;
            if (userData->noclipping != noclip) {
                manager.SetCollisionGroup(actor, noclip ? ecs::PhysicsGroup::NoClip : ecs::PhysicsGroup::Player);
                userData->noclipping = noclip;
                controller.pxController->invalidateCache();
            }

            auto a = PxExtendedVec3ToGlmVec3(controller.pxController->getFootPosition());
            auto b = transform.GetPosition();
            if (glm::any(glm::epsilonNotEqual(a, b, 0.00001f))) {
                Logf("Capsule out of sync: %s", glm::to_string(a - b));
            }

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

                controller.pxController->setHeight(currentHeight);
                setFootPosition(controller.pxController, transform.GetPosition());
            }

            // Update the capsule orientation
            glm::vec3 gravityForce = sceneProperties.fixedGravity;
            if (sceneProperties.gravityFunction) {
                auto gravityPos = sceneProperties.gravityTransform.GetInverse() *
                                  glm::vec4(getHeadPosition(controller.pxController), 1);
                gravityForce = sceneProperties.gravityTransform.GetRotation() *
                               sceneProperties.gravityFunction(gravityPos);
            }
            auto gravityStrength = glm::length(gravityForce);
            auto gravityDir = glm::normalize(gravityForce);
            if (gravityStrength > 0 && gravityStrength > CVarCharacterMinFlipGravity.Get()) {
                auto currentUp = transform.GetUp();
                auto angleDiff = glm::angle(currentUp, -gravityDir);
                if (angleDiff > 0) {
                    // TODO: Handle case where gravityDir == currentUp
                    auto rotationAxis = glm::normalize(glm::cross(currentUp, -gravityDir));
                    auto maxAngle = 10.0f; // glm::radians(CVarCharacterFlipSpeed.Get()) * dt;
                    auto deltaRotation = glm::angleAxis(std::min(angleDiff, maxAngle), rotationAxis);

                    glm::vec3 targetUpVector = deltaRotation * currentUp;
                    // if (angleDiff > maxAngle) {
                    //     targetUpVector = glm::normalize(glm::slerp(currentUp, -gravityDir, maxAngle / angleDiff));
                    // } else {
                    //     targetUpVector = -gravityDir;
                    // }

                    Logf("Angle diff: %s / %s = %f x %s (%s) up %s",
                        glm::to_string(currentUp),
                        glm::to_string(-gravityDir),
                        angleDiff,
                        glm::to_string(rotationAxis),
                        glm::to_string(deltaRotation),
                        glm::to_string(targetUpVector));

                    bool shouldRotate = true;
                    if (!noclip) {
                        auto halfHeight = controller.pxController->getHeight() * 0.5f;
                        auto currentOffset = currentUp * halfHeight;
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

                        transform.Rotate(deltaRotation);
                        transform.SetPosition(PxExtendedVec3ToGlmVec3(controller.pxController->getFootPosition()));

                        if (ecs::TransformTree::GetRoot(lock, head) != entity) {
                            // Rotate the head to match
                            auto targetTransform = transform * headRelativePlayer;
                            ecs::TransformTree::MoveViaRoot(lock, head, targetTransform);

                            // Logf("Rotating Head: %s, Up: %s, Forward: %s",
                            //     glm::to_string(targetTransform.GetPosition()),
                            //     glm::to_string(targetTransform.GetUp()),
                            //     glm::to_string(targetTransform.GetForward()));
                        }
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

            // Use head as a directional movement input
            glm::vec3 headInput = headRelativePlayer.GetPosition();
            headInput.y = 0;
            headInput = transform.GetRotation() * headInput;
            // Logf("Head input: %s", glm::to_string(headInput));

            // Update the capsule position, velocity, and onGround flag
            if (noclip) {
                auto movementVelocity = transform.GetRotation() * movementInput;
                transform.Translate(movementVelocity * dt + headInput);
                setFootPosition(controller.pxController, transform.GetPosition());

                // Move the head to the new player position
                rootTree.pose.Translate(movementVelocity * dt);

                userData->onGround = false;
                userData->actorData.velocity = movementVelocity;
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

                glm::vec3 velocityRelativePlayer, displacement;
                if (userData->onGround || inGround) {
                    velocityRelativePlayer = glm::inverse(transform.GetRotation()) * PxVec3ToGlmVec3(state.deltaXP);
                    velocityRelativePlayer += glm::vec3(movementInput.x, 0, movementInput.z);
                    auto relative = velocityRelativePlayer * dt;
                    if (jump) {
                        // Move up slightly first to detach the player from the floor
                        relative.y = std::max(0.0f, relative.y) + contactOffset;
                    } else {
                        // Always move down slightly for consistent onGround detection
                        relative.y = -contactOffset;
                    }
                    displacement = transform.GetRotation() * relative;
                } else {
                    auto worldMovement = transform.GetRotation() * movementInput;
                    userData->actorData.velocity += worldMovement * CVarCharacterAirStrafe.Get() * dt;
                    velocityRelativePlayer = glm::inverse(transform.GetRotation()) * userData->actorData.velocity;
                    displacement = userData->actorData.velocity * dt;
                }

                auto maxHeadInput = CVarCharacterMaxHeadSpeed.Get() * dt;
                headInput = glm::clamp(headInput, -maxHeadInput, maxHeadInput);
                // If the displacement is opposite headInput, make movementInput priority
                headInput -= glm::clamp(headInput, -glm::abs(displacement), glm::abs(displacement));

                // Logf("Disp: %s + %s, State:%u, On:%u, In:%u, DeltaXp: %s, Vel: %s",
                //     glm::to_string(displacement),
                //     glm::to_string(headInput),
                //     state.collisionFlags & PxControllerCollisionFlag::eCOLLISION_DOWN,
                //     userData->onGround,
                //     inGround,
                //     glm::to_string(PxVec3ToGlmVec3(state.deltaXP)),
                //     glm::to_string(userData->actorData.velocity));

                auto oldPosition = PxExtendedVec3ToGlmVec3(controller.pxController->getFootPosition());

                auto moveResult =
                    controller.pxController->move(GlmVec3ToPxVec3(displacement + headInput), 0, dt, moveQueryFilter);

                controller.pxController->getState(state);

                auto newPosition = PxExtendedVec3ToGlmVec3(controller.pxController->getFootPosition());

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
                        // When leaving a surface, use the velocity from the input state
                        userData->actorData.velocity = transform.GetRotation() * velocityRelativePlayer;
                        if (jump) userData->actorData.velocity -= gravityForce * CVarCharacterJumpHeight.Get();
                        // Logf("WasOn: %u, In: %u, Jump: %u, DeltaXp: %s, Vel: %s",
                        //     userData->onGround,
                        //     inGround,
                        //     jump,
                        //     glm::to_string(PxVec3ToGlmVec3(state.deltaXP)),
                        //     glm::to_string(userData->actorData.velocity));
                    } else {
                        userData->actorData.velocity = (newPosition - oldPosition - headInput) / dt;
                        userData->actorData.velocity += gravityForce * dt;
                        // Logf("OffGround, DeltaPos: %s, Disp %s, HeadInput: %s, Vel: %s",
                        //     glm::to_string(newPosition - oldPosition),
                        //     glm::to_string(displacement),
                        //     glm::to_string(headInput),
                        //     glm::to_string(userData->actorData.velocity));
                    }

                    userData->onGround = false;
                }

                // Move the entities to their new positions
                transform.SetPosition(newPosition);

                if (ecs::TransformTree::GetRoot(lock, head) != entity) {
                    // Subtract the head input from the movement without moving backwards.
                    // This allows the head to detatch from the player when colliding with walls.
                    auto deltaPos = newPosition - oldPosition;
                    deltaPos -= glm::clamp(headInput, -glm::abs(deltaPos), glm::abs(deltaPos));

                    auto targetTransform = headTree.GetGlobalTransform(lock);
                    targetTransform.Translate(deltaPos);
                    ecs::TransformTree::MoveViaRoot(lock, head, targetTransform);

                    // Logf("Moving Head: %s Up: %s, Forward: %s",
                    //     glm::to_string(targetTransform.GetPosition()),
                    //     glm::to_string(targetTransform.GetUp()),
                    //     glm::to_string(targetTransform.GetForward()));
                }
            }

            if (headRoot.Has<ecs::Physics>(lock) && manager.actors.count(headRoot) != 0) {
                auto &proxyActor = manager.actors[headRoot];
                if (proxyActor && proxyActor->userData) {
                    auto proxyUserData = (ActorUserData *)proxyActor->userData;
                    proxyUserData->velocity = userData->actorData.velocity;
                }
            }

            userData->actorData.pose = transform;
        }
    }
} // namespace sp
