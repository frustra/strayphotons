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

    void HumanControlSystem::Frame() {
        if (CVarPausePlayerPhysics.Get()) return;

        bool noclipChanged = CVarNoClip.Changed();
        auto noclip = CVarNoClip.Get(true);

        {
            auto lock = ecs::World.StartTransaction<ecs::Read<ecs::Name,
                                                        ecs::SignalOutput,
                                                        ecs::SignalBindings,
                                                        ecs::FocusLayer,
                                                        ecs::FocusLock,
                                                        ecs::PhysicsQuery>,
                ecs::Write<ecs::EventInput,
                    ecs::Transform,
                    ecs::HumanController,
                    ecs::InteractController,
                    ecs::Physics>>();

            for (Tecs::Entity entity : lock.EntitiesWith<ecs::InteractController>()) {
                auto &interact = entity.Get<ecs::InteractController>(lock);

                if (interact.target.Has<ecs::Physics>(lock)) {
                    auto &ph = interact.target.Get<ecs::Physics>(lock);
                    if (ph.constraint != entity) {
                        ph.group = ecs::PhysicsGroup::World;
                        interact.target = Tecs::Entity();
                    }
                }
            }

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
                    while (ecs::EventInput::Poll(lock, entity, INPUT_EVENT_CAMERA_ROTATE, event)) {
                        auto cursorDiff = std::get<glm::vec2>(event.data);
                        if (!rotating) {
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
                            if (!transform.HasChanged(userData->actorData.transformChangeNumber)) {
                                transform.SetRotation(rotation);
                                userData->actorData.transformChangeNumber = transform.ChangeNumber();
                            } else {
                                transform.SetRotation(rotation);
                            }
                        }
                    }
                }

                // Move the player
                if (noclipChanged) {
                    auto actor = controller.pxController->getActor();
                    manager.SetCollisionGroup(actor, noclip ? ecs::PhysicsGroup::NoClip : ecs::PhysicsGroup::Player);
                }

                auto currentHeight = controller.pxController->getHeight();
                auto targetHeight = crouching ? ecs::PLAYER_CAPSULE_CROUCH_HEIGHT : ecs::PLAYER_CAPSULE_HEIGHT;
                if (fabs(targetHeight - currentHeight) > 0.1) {
                    auto userData = (CharacterControllerUserData *)controller.pxController->getUserData();

                    // If player is in the air, resize from the top to implement crouch-jumping.
                    controller.height = currentHeight +
                                        (targetHeight - currentHeight) * (userData->onGround ? 0.1 : 1.0);
                }

                UpdatePlayerVelocity(lock, entity, inputMovement, jumping, sprinting, crouching);
                MoveEntity(lock, entity);
            }
        }
    }

    void HumanControlSystem::UpdatePlayerVelocity(
        ecs::Lock<ecs::Read<ecs::Transform>, ecs::Write<ecs::HumanController>> lock,
        Tecs::Entity entity,
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
            userData->velocity += movement * ecs::PLAYER_AIR_STRAFE * (float)(manager.interval.count() / 1e9);
            userData->velocity.y -= ecs::PLAYER_GRAVITY * (float)(manager.interval.count() / 1e9);
        }
    }

    void HumanControlSystem::MoveEntity(ecs::Lock<ecs::Write<ecs::Transform, ecs::HumanController>> lock,
        Tecs::Entity entity) {
        auto &transform = entity.Get<ecs::Transform>(lock);
        auto &controller = entity.Get<ecs::HumanController>(lock);

        if (controller.pxController) {
            auto userData = (CharacterControllerUserData *)controller.pxController->getUserData();
            if (transform.HasChanged(userData->actorData.transformChangeNumber)) return;

            auto disp = userData->velocity * (float)(manager.interval.count() / 1e9);
            auto prevPosition = PxExtendedVec3ToGlmVec3(controller.pxController->getPosition());
            glm::vec3 newPosition;
            if (CVarNoClip.Get()) {
                newPosition = prevPosition + disp;
                userData->onGround = true;
            } else {
                userData->onGround = manager.MoveController(controller.pxController,
                    manager.interval.count() / 1e9,
                    GlmVec3ToPxVec3(disp));
                newPosition = PxExtendedVec3ToGlmVec3(controller.pxController->getPosition());
            }

            if (!CVarNoClip.Get()) {
                // Update the velocity based on what happened in physx
                auto physxVelocity = (newPosition - prevPosition) / (float)(manager.interval.count() / 1e9);
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
} // namespace sp
