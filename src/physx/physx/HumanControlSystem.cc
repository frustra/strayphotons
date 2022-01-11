#include "HumanControlSystem.hh"

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
    static CVar<bool> CVarNoClip("p.NoClip", false, "Disable player clipping");
    static CVar<bool> CVarPausePlayerPhysics("p.PausePlayerPhysics", false, "Disable player physics update");
    static CVar<float> CVarMovementSpeed("p.MovementSpeed", 3.0, "Player walking movement speed (m/s)");
    static CVar<float> CVarSprintSpeed("p.SprintSpeed", 6.0, "Player sprinting movement speed (m/s)");
    static CVar<float> CVarCrouchSpeed("p.CrouchSpeed", 1.5, "Player crouching movement speed (m/s)");
    static CVar<float> CVarCursorSensitivity("p.CursorSensitivity", 1.0, "Mouse cursor sensitivity");

    void HumanControlSystem::Frame(double dtSinceLastFrame) {
        if (CVarPausePlayerPhysics.Get()) return;

        bool noclipChanged = CVarNoClip.Changed();
        auto noclip = CVarNoClip.Get(true);

        {
            auto lock = ecs::World.StartTransaction<
                ecs::Read<ecs::Name, ecs::SignalOutput, ecs::SignalBindings, ecs::FocusLayer, ecs::FocusLock>,
                ecs::Write<ecs::EventInput,
                    ecs::Transform,
                    ecs::HumanController,
                    ecs::InteractController,
                    ecs::Physics>>();

            for (Tecs::Entity entity : lock.EntitiesWith<ecs::HumanController>()) {
                if (!entity.Has<ecs::Transform>(lock)) continue;

                auto &controller = entity.Get<ecs::HumanController>(lock);
                if (!controller.pxController) continue;

                // Handle keyboard controls
                glm::vec3 inputMovement = glm::vec3(0);
                bool jumping = false;
                bool sprinting = false;
                bool crouching = false;
                bool rotating = false;

                inputMovement.z -= ecs::SignalBindings::GetSignal(lock, entity, INPUT_SIGNAL_MOVE_FORWARD);
                inputMovement.z += ecs::SignalBindings::GetSignal(lock, entity, INPUT_SIGNAL_MOVE_BACK);
                inputMovement.x -= ecs::SignalBindings::GetSignal(lock, entity, INPUT_SIGNAL_MOVE_LEFT);
                inputMovement.x += ecs::SignalBindings::GetSignal(lock, entity, INPUT_SIGNAL_MOVE_RIGHT);

                if (noclip) {
                    inputMovement.y += ecs::SignalBindings::GetSignal(lock, entity, INPUT_SIGNAL_MOVE_JUMP);
                    inputMovement.y -= ecs::SignalBindings::GetSignal(lock, entity, INPUT_SIGNAL_MOVE_CROUCH);
                } else {
                    jumping = ecs::SignalBindings::GetSignal(lock, entity, INPUT_SIGNAL_MOVE_JUMP) >= 0.5;
                    crouching = ecs::SignalBindings::GetSignal(lock, entity, INPUT_SIGNAL_MOVE_CROUCH) >= 0.5;
                }
                sprinting = ecs::SignalBindings::GetSignal(lock, entity, INPUT_SIGNAL_MOVE_SPRINT) >= 0.5;
                rotating = ecs::SignalBindings::GetSignal(lock, entity, INPUT_SIGNAL_INTERACT_ROTATE) >= 0.5;

                inputMovement.x = std::clamp(inputMovement.x, -1.0f, 1.0f);
                inputMovement.y = std::clamp(inputMovement.y, -1.0f, 1.0f);
                inputMovement.z = std::clamp(inputMovement.z, -1.0f, 1.0f);

                if (entity.Has<ecs::EventInput>(lock)) {
                    ecs::Event event;
                    while (ecs::EventInput::Poll(lock, entity, INPUT_EVENT_INTERACT, event)) {
                        Interact(lock, entity);
                    }

                    while (ecs::EventInput::Poll(lock, entity, INPUT_EVENT_CAMERA_ROTATE, event)) {
                        auto cursorDiff = std::get<glm::vec2>(event.data);
                        if (!rotating || !InteractRotate(lock, entity, cursorDiff)) {
                            float sensitivity = CVarCursorSensitivity.Get() * 0.001;

                            // Apply pitch/yaw rotations
                            auto &transform = entity.Get<ecs::Transform>(lock);
                            auto rotation = glm::quat(glm::vec3(0, -cursorDiff.x * sensitivity, 0)) *
                                            transform.GetRotation() *
                                            glm::quat(glm::vec3(-cursorDiff.y * sensitivity, 0, 0));

                            auto up = rotation * glm::vec3(0, 1, 0);
                            if (up.y < 0) {
                                // Camera is turning upside-down, reset it
                                auto right = rotation * glm::vec3(1, 0, 0);
                                right.y = 0;
                                up.y = 0;
                                glm::vec3 forward = glm::cross(right, up);
                                rotation = glm::quat_cast(
                                    glm::mat3(glm::normalize(right), glm::normalize(up), glm::normalize(forward)));
                            }
                            auto userData = (CharacterControllerUserData *)controller.pxController->getUserData();
                            if (!transform.HasChanged(userData->transformChangeNumber)) {
                                transform.SetRotation(rotation);
                                userData->transformChangeNumber = transform.ChangeNumber();
                            } else {
                                transform.SetRotation(rotation);
                            }
                        }
                    }
                }

                // Move the player
                if (noclipChanged) {
                    physics->EnableCollisions(controller.pxController->getActor(), !noclip);

                    physx::PxShape *shape;
                    controller.pxController->getActor()->getShapes(&shape, 1);
                    physx::PxFilterData data;
                    data.word0 = noclip ? PhysxCollisionGroup::NOCLIP : PhysxCollisionGroup::PLAYER;
                    shape->setQueryFilterData(data);
                    shape->setSimulationFilterData(data);
                }

                auto currentHeight = controller.pxController->getHeight();
                auto targetHeight = crouching ? ecs::PLAYER_CAPSULE_CROUCH_HEIGHT : ecs::PLAYER_CAPSULE_HEIGHT;
                if (fabs(targetHeight - currentHeight) > 0.1) {
                    auto userData = (CharacterControllerUserData *)controller.pxController->getUserData();

                    // If player is in the air, resize from the top to implement crouch-jumping.
                    controller.height = currentHeight +
                                        (targetHeight - currentHeight) * (userData->onGround ? 0.1 : 1.0);
                }

                UpdatePlayerVelocity(lock, entity, dtSinceLastFrame, inputMovement, jumping, sprinting, crouching);
                MoveEntity(lock, entity, dtSinceLastFrame);
            }
        }
    }

    void HumanControlSystem::UpdatePlayerVelocity(
        ecs::Lock<ecs::Read<ecs::Transform>, ecs::Write<ecs::HumanController>> lock,
        Tecs::Entity entity,
        double dtSinceLastFrame,
        glm::vec3 inDirection,
        bool jump,
        bool sprint,
        bool crouch) {
        Assert(entity.Has<ecs::Transform>(lock), "Entity must have a Transform component");

        auto &controller = entity.Get<ecs::HumanController>(lock);
        auto userData = (CharacterControllerUserData *)controller.pxController->getUserData();

        auto &transform = entity.Get<ecs::Transform>(lock);
        glm::vec3 movement = transform.GetRotation() * glm::vec3(inDirection.x, 0, inDirection.z);

        auto noclip = CVarNoClip.Get();
        if (!noclip) {
            if (std::abs(movement.y) > 0.999) { movement = transform.GetRotation() * glm::vec3(0, -movement.y, 0); }
            movement.y = 0;
        }
        if (movement != glm::vec3(0)) {
            float speed = CVarMovementSpeed.Get();
            if (sprint && userData->onGround) speed = CVarSprintSpeed.Get();
            if (crouch && userData->onGround) speed = CVarCrouchSpeed.Get();
            movement = glm::normalize(movement) * speed;
        }
        movement.y += inDirection.y * CVarMovementSpeed.Get();

        if (noclip) {
            userData->velocity = movement;
            return;
        }
        if (userData->onGround) {
            userData->velocity.x = movement.x;
            userData->velocity.y -= 0.01; // Always try moving down so that onGround detection is more consistent.
            if (jump) userData->velocity.y = ecs::PLAYER_JUMP_VELOCITY;
            userData->velocity.z = movement.z;
        } else {
            userData->velocity += movement * ecs::PLAYER_AIR_STRAFE * (float)dtSinceLastFrame;
            userData->velocity.y -= ecs::PLAYER_GRAVITY * dtSinceLastFrame;
        }
    }

    void HumanControlSystem::MoveEntity(ecs::Lock<ecs::Write<ecs::Transform, ecs::HumanController>> lock,
        Tecs::Entity entity,
        double dtSinceLastFrame) {
        auto &transform = entity.Get<ecs::Transform>(lock);
        auto &controller = entity.Get<ecs::HumanController>(lock);

        if (controller.pxController) {
            auto userData = (CharacterControllerUserData *)controller.pxController->getUserData();
            if (transform.HasChanged(userData->transformChangeNumber)) return;

            auto disp = userData->velocity * (float)dtSinceLastFrame;
            auto prevPosition = PxExtendedVec3ToGlmVec3(controller.pxController->getPosition());
            glm::vec3 newPosition;
            if (CVarNoClip.Get()) {
                newPosition = prevPosition + disp;
                userData->onGround = true;
            } else {
                userData->onGround = physics->MoveController(controller.pxController,
                    dtSinceLastFrame,
                    GlmVec3ToPxVec3(disp));
                newPosition = PxExtendedVec3ToGlmVec3(controller.pxController->getPosition());
            }

            if (!CVarNoClip.Get()) {
                // Update the velocity based on what happened in physx
                auto physxVelocity = (newPosition - prevPosition) / (float)dtSinceLastFrame;
                auto inputSpeed = glm::length(userData->velocity);
                if (glm::length(physxVelocity) > inputSpeed) {
                    // Don't allow the physics simulation to accelerate the character faster than the input speed
                    userData->velocity = glm::normalize(physxVelocity) * inputSpeed;
                } else {
                    userData->velocity = physxVelocity;
                }
            } else {
                userData->velocity = glm::vec3(0);
            }

            // Offset the capsule position so the camera is at the top
            float capsuleHeight = controller.pxController->getHeight();
            transform.SetPosition(newPosition + glm::vec3(0, capsuleHeight / 2, 0));
        }
    }

    void HumanControlSystem::Interact(
        ecs::Lock<ecs::Read<ecs::HumanController, ecs::Transform>, ecs::Write<ecs::InteractController, ecs::Physics>>
            lock,
        Tecs::Entity entity) {
        auto &interact = entity.Get<ecs::InteractController>(lock);
        auto transform = entity.Get<ecs::Transform>(lock).GetGlobalTransform(lock);

        if (interact.target.Has<ecs::Physics>(lock)) {
            auto &ph = interact.target.Get<ecs::Physics>(lock);
            ph.constraint = Tecs::Entity();
            ph.constraintOffset = glm::vec3();
            ph.constraintRotation = glm::quat();
            physics->SetCollisionGroup(ph.actor, PhysxCollisionGroup::WORLD);
            interact.target = Tecs::Entity();
            return;
        }

        glm::vec3 origin = transform.GetPosition();
        glm::vec3 dir = transform.GetForward();

        physx::PxReal maxDistance = 2.0f;

        physx::PxRaycastBuffer hit;
        bool status = physics->RaycastQuery(lock, entity, origin, dir, maxDistance, hit);

        if (status) {
            physx::PxRigidActor *hitActor = hit.block.actor;
            if (hitActor && hitActor->is<physx::PxRigidDynamic>()) {
                physx::PxRigidDynamic *dynamic = static_cast<physx::PxRigidDynamic *>(hitActor);
                if (!dynamic->getRigidBodyFlags().isSet(physx::PxRigidBodyFlag::eKINEMATIC)) {

                    auto userData = (ActorUserData *)hitActor->userData;
                    if (userData != nullptr && userData->entity.Has<ecs::Physics, ecs::Transform>(lock)) {
                        auto &hitPhysics = userData->entity.Get<ecs::Physics>(lock);
                        auto hitTransform = userData->entity.Get<ecs::Transform>(lock).GetGlobalTransform(lock);

                        interact.target = userData->entity;
                        auto currentPos = hitTransform.GetTransform() *
                                          glm::vec4(PxVec3ToGlmVec3(dynamic->getCMassLocalPose().p), 1.0f);
                        auto invParentRotate = glm::inverse(transform.GetRotation());

                        physics->SetCollisionGroup(hitActor, PhysxCollisionGroup::HELD_OBJECT);
                        hitPhysics.constraint = entity;
                        hitPhysics.constraintOffset = invParentRotate * (currentPos - origin + glm::vec3(0, 0.1, 0));
                        hitPhysics.constraintRotation = invParentRotate * hitTransform.GetRotation();
                    }
                }
            }
        }
    }

    bool HumanControlSystem::InteractRotate(
        ecs::Lock<ecs::Read<ecs::InteractController, ecs::Transform>, ecs::Write<ecs::Physics>> lock,
        Tecs::Entity entity,
        glm::vec2 dCursor) {
        if (!entity.Has<ecs::InteractController, ecs::Transform>(lock)) return false;
        auto &interact = entity.Get<ecs::InteractController>(lock);
        auto &parentTransform = entity.Get<ecs::Transform>(lock);
        if (!interact.target.Has<ecs::Physics>(lock)) return false;
        auto &ph = interact.target.Get<ecs::Physics>(lock);

        auto input = dCursor * CVarCursorSensitivity.Get() * 0.01f;
        auto upAxis = glm::inverse(parentTransform.GetGlobalRotation(lock)) * glm::vec3(0, 1, 0);
        ph.constraintRotation = ph.constraintRotation * glm::quat(input.x, upAxis);
        // ph.constraintRotation = glm::quat(input.y, glm::vec3(1, 0, 0)) * ph.constraintRotation;
        return true;
    }
} // namespace sp
