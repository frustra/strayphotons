#include "HumanControlSystem.hh"

#include "core/CVar.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/Ecs.hh"
#include "ecs/EcsImpl.hh"
#include "input/core/BindingNames.hh"
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
                ecs::Write<ecs::EventInput, ecs::Transform, ecs::HumanController, ecs::InteractController>>();

            for (Tecs::Entity entity : lock.EntitiesWith<ecs::HumanController>()) {
                if (!entity.Has<ecs::Transform>(lock)) continue;

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
                    inputMovement.y -= ecs::SignalBindings::GetSignal(lock, entity, INPUT_SIGNAL_MOVE_JUMP);
                    inputMovement.y += ecs::SignalBindings::GetSignal(lock, entity, INPUT_SIGNAL_MOVE_CROUCH);
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
                                            transform.GetRotate() *
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
                            transform.SetRotate(rotation, false);
                        }
                    }
                }

                auto &controller = entity.Get<ecs::HumanController>(lock);
                if (!controller.pxController) continue;

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
                    // If player is in the air, resize from the top to implement crouch-jumping.
                    controller.height = currentHeight +
                                        (targetHeight - currentHeight) * (controller.onGround ? 0.1 : 1.0);
                }

                auto velocity = CalculatePlayerVelocity(lock,
                                                        entity,
                                                        dtSinceLastFrame,
                                                        inputMovement,
                                                        jumping,
                                                        sprinting,
                                                        crouching);
                MoveEntity(lock, entity, dtSinceLastFrame, velocity);
            }
        }
    }

    glm::vec3 HumanControlSystem::CalculatePlayerVelocity(
        ecs::Lock<ecs::Read<ecs::Transform>, ecs::Write<ecs::HumanController>> lock,
        Tecs::Entity entity,
        double dtSinceLastFrame,
        glm::vec3 inDirection,
        bool jump,
        bool sprint,
        bool crouch) {
        if (!entity.Has<ecs::Transform>(lock)) {
            throw std::invalid_argument("entity must have a Transform component");
        }

        auto noclip = CVarNoClip.Get();
        auto &controller = entity.Get<ecs::HumanController>(lock);
        auto &transform = entity.Get<ecs::Transform>(lock);

        glm::vec3 movement = transform.GetRotate() * glm::vec3(inDirection.x, 0, inDirection.z);

        if (!noclip) {
            if (std::abs(movement.y) > 0.999) { movement = transform.GetRotate() * glm::vec3(0, -movement.y, 0); }
            movement.y = 0;
        }
        if (movement != glm::vec3(0)) {
            float speed = CVarMovementSpeed.Get();
            if (sprint && controller.onGround) speed = CVarSprintSpeed.Get();
            if (crouch && controller.onGround) speed = CVarCrouchSpeed.Get();
            movement = glm::normalize(movement) * speed;
        }
        movement.y += inDirection.y * CVarMovementSpeed.Get();

        if (noclip) {
            controller.velocity = movement;
            return controller.velocity;
        }
        if (controller.onGround) {
            controller.velocity.x = movement.x;
            controller.velocity.y -= 0.01; // Always try moving down so that onGround detection is more consistent.
            if (jump) controller.velocity.y = ecs::PLAYER_JUMP_VELOCITY;
            controller.velocity.z = movement.z;
        } else {
            controller.velocity += movement * ecs::PLAYER_AIR_STRAFE * (float)dtSinceLastFrame;
            controller.velocity.y -= ecs::PLAYER_GRAVITY * dtSinceLastFrame;
        }

        return controller.velocity;
    }

    void HumanControlSystem::MoveEntity(ecs::Lock<ecs::Write<ecs::Transform, ecs::HumanController>> lock,
                                        Tecs::Entity entity,
                                        double dtSinceLastFrame,
                                        glm::vec3 velocity) {
        auto &transform = entity.Get<ecs::Transform>(lock);
        if (transform.IsDirty()) return;
        auto &controller = entity.Get<ecs::HumanController>(lock);

        if (controller.pxController) {
            auto disp = velocity * (float)dtSinceLastFrame;
            auto prevPosition = PxExtendedVec3ToGlmVec3P(controller.pxController->getPosition());
            glm::vec3 newPosition;
            if (CVarNoClip.Get()) {
                newPosition = prevPosition + disp;
                controller.onGround = true;
            } else {
                controller.onGround = physics->MoveController(controller.pxController,
                                                              dtSinceLastFrame,
                                                              GlmVec3ToPxVec3(disp));
                newPosition = PxExtendedVec3ToGlmVec3P(controller.pxController->getPosition());
            }
            // Don't accelerate more than our current velocity
            auto velocityPosition = glm::min(prevPosition + glm::abs(disp), newPosition);
            velocityPosition = glm::max(prevPosition - glm::abs(disp), velocityPosition);

            // Update the velocity based on what happened in physx
            controller.velocity = (velocityPosition - prevPosition) / (float)dtSinceLastFrame;
            glm::vec3 *velocity = (glm::vec3 *)controller.pxController->getUserData();
            *velocity = CVarNoClip.Get() ? glm::vec3(0) : controller.velocity;

            // Offset the capsule position so the camera is at the top
            float capsuleHeight = controller.pxController->getHeight();
            transform.SetPosition(newPosition + glm::vec3(0, capsuleHeight / 2, 0));
        }
    }

    void HumanControlSystem::Interact(
        ecs::Lock<ecs::Read<ecs::HumanController>, ecs::Write<ecs::Transform, ecs::InteractController>> lock,
        Tecs::Entity entity) {
        auto &interact = entity.Get<ecs::InteractController>(lock);
        auto &transform = entity.Get<ecs::Transform>(lock);

        if (interact.target) {
            physics->RemoveConstraint(entity, interact.target);
            interact.target = nullptr;
            return;
        }

        glm::vec3 origin = transform.GetPosition();
        glm::vec3 dir = glm::normalize(transform.GetForward());

        physx::PxReal maxDistance = 2.0f;

        physx::PxRaycastBuffer hit;
        bool status = physics->RaycastQuery(lock, entity, origin, dir, maxDistance, hit);

        if (status) {
            physx::PxRigidActor *hitActor = hit.block.actor;
            if (hitActor && hitActor->getType() == physx::PxActorType::eRIGID_DYNAMIC) {
                physx::PxRigidDynamic *dynamic = static_cast<physx::PxRigidDynamic *>(hitActor);
                if (dynamic && !dynamic->getRigidBodyFlags().isSet(physx::PxRigidBodyFlag::eKINEMATIC)) {
                    interact.target = dynamic;
                    auto pose = dynamic->getGlobalPose();
                    auto currentPos = PxVec3ToGlmVec3P(
                        pose.transform(dynamic->getCMassLocalPose().transform(physx::PxVec3(0.0))));
                    auto invRotate = glm::inverse(transform.GetRotate());
                    auto offset = invRotate * (currentPos - origin + glm::vec3(0, 0.1, 0));
                    physics->CreateConstraint(lock,
                                              entity,
                                              dynamic,
                                              GlmVec3ToPxVec3(offset),
                                              GlmQuatToPxQuat(invRotate) * pose.q);
                }
            }
        }
    }

    bool HumanControlSystem::InteractRotate(ecs::Lock<ecs::Read<ecs::InteractController>> lock,
                                            Tecs::Entity entity,
                                            glm::vec2 dCursor) {
        auto &interact = entity.Get<ecs::InteractController>(lock);
        if (interact.target) {
            auto rotation = glm::vec3(dCursor.y, dCursor.x, 0) * (float)(CVarCursorSensitivity.Get() * 0.01);
            physics->RotateConstraint(entity, interact.target, GlmVec3ToPxVec3(rotation));
            return true;
        }
        return false;
    }
} // namespace sp
