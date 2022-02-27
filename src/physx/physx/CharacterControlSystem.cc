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
#include <sstream>

namespace sp {
    using namespace physx;

    static CVar<float> CVarCharacterMovementSpeed("p.CharacterMovementSpeed",
        3.0,
        "Character controller movement speed (m/s)");
    static CVar<float> CVarCharacterSprintSpeed("p.CharacterSprintSpeed",
        5.0,
        "Character controller sprint speed (m/s)");
    static CVar<bool> CVarPropJumping("x.PropJumping", false, "Disable player collision with held object");

    CharacterControlSystem::CharacterControlSystem(PhysxManager &manager) : manager(manager) {
        auto lock = ecs::World.StartTransaction<ecs::AddRemove>();
        characterControllerObserver = lock.Watch<ecs::ComponentEvent<ecs::CharacterController>>();
    }

    void CharacterControlSystem::Frame(
        ecs::Lock<ecs::Read<ecs::Name, ecs::SignalOutput, ecs::SignalBindings, ecs::FocusLayer, ecs::FocusLock>,
            ecs::Write<ecs::TransformTree, ecs::EventInput, ecs::CharacterController>> lock) {
        // Update PhysX with any added or removed CharacterControllers
        ecs::ComponentEvent<ecs::CharacterController> controllerEvent;
        while (characterControllerObserver.Poll(lock, controllerEvent)) {
            if (controllerEvent.type == Tecs::EventType::ADDED) {
                if (controllerEvent.entity.Has<ecs::CharacterController>(lock)) {
                    auto &controller = controllerEvent.entity.Get<ecs::CharacterController>(lock);
                    if (!controller.pxController) {
                        auto characterUserData = new CharacterControllerUserData(controllerEvent.entity);

                        PxCapsuleControllerDesc desc;
                        desc.position = PxExtendedVec3(0, 0, 0);
                        desc.upDirection = PxVec3(0, 1, 0);
                        desc.radius = ecs::PLAYER_RADIUS;
                        desc.height = ecs::PLAYER_CAPSULE_HEIGHT;
                        desc.stepOffset = ecs::PLAYER_STEP_HEIGHT;
                        desc.scaleCoeff = 1.0f; // Why is the default 0.8? No idea...
                        desc.contactOffset = 0.01f;
                        desc.material = manager.pxPhysics->createMaterial(0.3f, 0.3f, 0.3f);
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
        if (CVarPropJumping.Get()) {
            filterData.word0 = ecs::PHYSICS_GROUP_WORLD | ecs::PHYSICS_GROUP_PLAYER_HANDS;
        } else {
            filterData.word0 = ecs::PHYSICS_GROUP_WORLD;
        }
        PxControllerFilters moveQueryFilter(&filterData);

        float dt = (float)(manager.interval.count() / 1e9);

        for (auto &entity : lock.EntitiesWith<ecs::CharacterController>()) {
            if (!entity.Has<ecs::CharacterController, ecs::TransformTree>(lock)) continue;

            auto &controller = entity.Get<ecs::CharacterController>(lock);
            if (!controller.pxController) continue;
            auto &transformTree = entity.Get<ecs::TransformTree>(lock);
            Assertf(!transformTree.parent,
                "CharacterController should not have a TransformTree parent: %s",
                ecs::ToString(lock, entity));
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
                targetPosition.y = transform.GetPosition().y;
                targetHeight = std::max(0.1f, targetTree.pose.GetPosition().y - ecs::PLAYER_RADIUS);
            }

            // If the origin moved, teleport the controller
            if (transform.matrix != userData->actorData.pose.matrix) {
                controller.pxController->setHeight(targetHeight);
                controller.pxController->setFootPosition(GlmVec3ToPxExtendedVec3(targetPosition));

                // Updating the controller position does not update the underlying actor, we need to do it ourselves.
                auto capsulePosition = controller.pxController->getPosition();
                auto actorPose = actor->getGlobalPose();
                actorPose.p = PxVec3(capsulePosition.x, capsulePosition.y, capsulePosition.z);
                actor->setGlobalPose(actorPose);

                userData->onGround = false;
                userData->actorData.pose = ecs::Transform(targetPosition);
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
            glm::vec3 lateralMovement = glm::vec3(0);
            lateralMovement.x = ecs::SignalBindings::GetSignal(lock, entity, INPUT_SIGNAL_MOVE_WORLD_X);
            lateralMovement.z = ecs::SignalBindings::GetSignal(lock, entity, INPUT_SIGNAL_MOVE_WORLD_Z);
            float verticalMovement = ecs::SignalBindings::GetSignal(lock, entity, INPUT_SIGNAL_MOVE_WORLD_Y);
            bool sprint = ecs::SignalBindings::GetSignal(lock, entity, INPUT_SIGNAL_MOVE_SPRINT) >= 0.5;
            bool noclip = ecs::SignalBindings::GetSignal(lock, entity, INPUT_SIGNAL_MOVE_NOCLIP) >= 0.5;

            bool jump = false;
            if (entity.Has<ecs::EventInput>(lock)) {
                auto &eventInput = entity.Get<ecs::EventInput>(lock);
                ecs::Event event;
                while (eventInput.Poll("/action/jump", event)) {
                    jump = true;
                }
            }

            float speed = sprint ? CVarCharacterSprintSpeed.Get() : CVarCharacterMovementSpeed.Get();
            if (lateralMovement != glm::vec3(0)) lateralMovement = glm::normalize(lateralMovement) * speed;
            verticalMovement = std::clamp(verticalMovement, -1.0f, 1.0f) * speed;

            auto targetDelta = targetPosition - userData->actorData.pose.GetPosition();

            if (userData->noclipping != noclip) {
                manager.SetCollisionGroup(actor, noclip ? ecs::PhysicsGroup::NoClip : ecs::PhysicsGroup::Player);
                userData->noclipping = noclip;
            }

            // Update the capsule position, velocity, and onGround flag
            if (noclip) {
                targetPosition += lateralMovement * dt;
                targetPosition.y += verticalMovement * dt;
                controller.pxController->setFootPosition(GlmVec3ToPxExtendedVec3(targetPosition + targetDelta));

                userData->onGround = false;
                userData->actorData.velocity = lateralMovement;
                userData->actorData.velocity.y = verticalMovement;
            } else {
                PxControllerState state;
                controller.pxController->getState(state);

                // If player is moving up, onGround detection does not work, so we need to do it ourselves.
                // This edge-case is possible when jumping in an elevator; the floor can catch up to the player
                // while their velocity is still in the up direction.
                PxOverlapHit touch;
                PxOverlapBuffer hit;
                hit.touches = &touch;
                hit.maxNbTouches = 1;
                PxCapsuleGeometry capsuleGeometry(ecs::PLAYER_RADIUS, currentHeight * 0.5f);
                bool inGround = manager.scene->overlap(capsuleGeometry,
                    actor->getGlobalPose(),
                    hit,
                    PxQueryFilterData(filterData, PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC));

                glm::vec3 displacement;
                if (userData->onGround || inGround) {
                    displacement = (PxVec3ToGlmVec3(state.deltaXP) + lateralMovement) * dt;
                    if (jump) {
                        // Move up slightly first to detach the player from the floor
                        displacement.y = std::max(state.deltaXP.y * dt, 0.0f) + contactOffset;
                    } else {
                        // Always move down slightly for consistent onGround detection
                        displacement.y = -contactOffset;
                    }
                } else {
                    userData->actorData.velocity += lateralMovement * ecs::PLAYER_AIR_STRAFE * dt;
                    displacement = userData->actorData.velocity * dt;
                }
                displacement += targetDelta;

                auto moveResult = controller.pxController->move(GlmVec3ToPxVec3(displacement), 0, dt, moveQueryFilter);

                if (moveResult & PxControllerCollisionFlag::eCOLLISION_DOWN) {
                    userData->actorData.velocity = PxVec3ToGlmVec3(state.deltaXP);
                    userData->onGround = true;
                } else {
                    if (userData->onGround || inGround) {
                        userData->actorData.velocity = PxVec3ToGlmVec3(state.deltaXP);
                        userData->actorData.velocity.x += displacement.x / dt;
                        userData->actorData.velocity.z += displacement.z / dt;
                        if (jump) userData->actorData.velocity.y += ecs::PLAYER_JUMP_VELOCITY;
                    } else {
                        auto deltaPos = PxExtendedVec3ToGlmVec3(controller.pxController->getFootPosition()) -
                                        userData->actorData.pose.GetPosition() - targetDelta;
                        userData->actorData.velocity = deltaPos / dt;
                        userData->actorData.velocity.y -= ecs::PLAYER_GRAVITY * dt;
                    }

                    userData->onGround = false;
                }
            }

            auto newPosition = PxExtendedVec3ToGlmVec3(controller.pxController->getFootPosition());
            auto movementProxy = controller.movementProxy.Get(lock);
            if (movementProxy.Has<ecs::TransformTree>(lock)) {
                auto &proxyTransform = movementProxy.Get<ecs::TransformTree>(lock);
                proxyTransform.pose.Translate(newPosition - userData->actorData.pose.GetPosition() - targetDelta);
            }

            transform.SetPosition(newPosition);
            userData->actorData.pose = transform;
        }
    }
} // namespace sp
