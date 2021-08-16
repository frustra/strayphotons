#include "HumanControlSystem.hh"

#include "core/CVar.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/Ecs.hh"
#include "ecs/EcsImpl.hh"
#include "input/InputManager.hh"
#include "physx/PhysxManager.hh"
#include "physx/PhysxUtils.hh"

#include <PxRigidActor.h>
#include <PxScene.h>
#include <cmath>
#include <glm/glm.hpp>
#include <sstream>

namespace sp {
    static CVar<bool> CVarNoClip("p.NoClip", false, "Disable player clipping");
    static CVar<float> CVarMovementSpeed("p.MovementSpeed", 3.0, "Player walking movement speed (m/s)");
    static CVar<float> CVarSprintSpeed("p.SprintSpeed", 6.0, "Player sprinting movement speed (m/s)");
    static CVar<float> CVarCrouchSpeed("p.CrouchSpeed", 1.5, "Player crouching movement speed (m/s)");
    static CVar<float> CVarCursorSensitivity("p.CursorSensitivity", 1.0, "Mouse cursor sensitivity");

    HumanControlSystem::HumanControlSystem(ecs::ECS &ecs, InputManager *input, PhysxManager *physics)
        : ecs(ecs), input(input), physics(physics) {}

    HumanControlSystem::~HumanControlSystem() {}

    bool HumanControlSystem::Frame(double dtSinceLastFrame) {
        if (input != nullptr && input->FocusLocked()) return true;

        bool noclipChanged = CVarNoClip.Changed();
        auto noclip = CVarNoClip.Get(true);

        {
            auto lock = ecs.StartTransaction<
                ecs::Write<ecs::PhysicsState, ecs::Transform, ecs::HumanController, ecs::InteractController>>();

            for (Tecs::Entity entity : lock.EntitiesWith<ecs::HumanController>()) {
                if (!entity.Has<ecs::Transform>(lock)) continue;

                // Handle keyboard controls
                glm::vec3 inputMovement = glm::vec3(0);
                bool jumping = false;
                bool sprinting = false;
                bool crouching = false;
                bool rotating = false;

                auto &controller = entity.Get<ecs::HumanController>(lock);

                if (input != nullptr) {
                    if (input->IsDown(INPUT_ACTION_PLAYER_MOVE_FORWARD)) { inputMovement += glm::vec3(0, 0, -1); }

                    if (input->IsDown(INPUT_ACTION_PLAYER_MOVE_BACKWARD)) { inputMovement += glm::vec3(0, 0, 1); }

                    if (input->IsDown(INPUT_ACTION_PLAYER_MOVE_LEFT)) { inputMovement += glm::vec3(-1, 0, 0); }

                    if (input->IsDown(INPUT_ACTION_PLAYER_MOVE_RIGHT)) { inputMovement += glm::vec3(1, 0, 0); }

                    if (input->IsDown(INPUT_ACTION_PLAYER_MOVE_JUMP)) {
                        if (noclip) {
                            inputMovement += glm::vec3(0, 1, 0);
                        } else {
                            jumping = true;
                        }
                    }

                    if (input->IsDown(INPUT_ACTION_PLAYER_MOVE_CROUCH)) {
                        if (noclip) {
                            inputMovement += glm::vec3(0, -1, 0);
                        } else {
                            crouching = true;
                        }
                    }

                    if (input->IsDown(INPUT_ACTION_PLAYER_MOVE_SPRINT)) { sprinting = true; }

                    if (input->IsPressed(INPUT_ACTION_PLAYER_INTERACT)) { Interact(lock, entity); }

                    if (input->IsDown(INPUT_ACTION_PLAYER_INTERACT_ROTATE)) { rotating = true; }

                    // Handle mouse controls
                    const glm::vec2 *cursorPos, *cursorPosPrev;
                    if (input->GetActionDelta(INPUT_ACTION_MOUSE_CURSOR, &cursorPos, &cursorPosPrev)) {
                        glm::vec2 cursorDiff = *cursorPos;
                        if (cursorPosPrev != nullptr) { cursorDiff -= *cursorPosPrev; }
                        if (!rotating || !InteractRotate(lock, entity, dtSinceLastFrame, cursorDiff)) {
                            float sensitivity = CVarCursorSensitivity.Get() * 0.001;

                            auto &transform = entity.Get<ecs::Transform>(lock);
                            auto rotation = transform.GetRotate();
                            // Determine current pitch/yaw
                            float pitch = glm::pitch(rotation);
                            if (pitch > M_PI) pitch -= M_PI * 2;

                            float yaw = glm::yaw(rotation);

                            if (std::abs(glm::roll(rotation)) > std::numeric_limits<float>::epsilon()) {
                                pitch += (pitch > 0) ? -M_PI : M_PI;
                                yaw = M_PI - yaw;
                            }

                            // Update pitch/yaw
                            yaw -= cursorDiff.x * sensitivity;
                            if (yaw > 2.0f * M_PI) { yaw -= 2.0f * M_PI; }
                            if (yaw < 0) { yaw += 2.0f * M_PI; }

                            pitch -= cursorDiff.y * sensitivity;

                            const float feps = std::numeric_limits<float>::epsilon();
                            pitch = std::max(-((float)M_PI_2 - feps), std::min(pitch, (float)M_PI_2 - feps));

                            transform.SetRotate(glm::quat(glm::vec3(pitch, yaw, 0)));
                        }
                    }
                }

                // Move the player
                if (noclipChanged) {
                    physics->EnableCollisions(lock, controller.pxController->getActor(), !noclip);

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

        return true;
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

    void HumanControlSystem::MoveEntity(
        ecs::Lock<ecs::Write<ecs::PhysicsState, ecs::Transform, ecs::HumanController>> lock,
        Tecs::Entity entity,
        double dtSinceLastFrame,
        glm::vec3 velocity) {
        auto &transform = entity.Get<ecs::Transform>(lock);
        auto &controller = entity.Get<ecs::HumanController>(lock);

        if (controller.pxController) {
            auto disp = velocity * (float)dtSinceLastFrame;
            auto prevPosition = PxExtendedVec3ToGlmVec3P(controller.pxController->getPosition());
            if (CVarNoClip.Get()) {
                transform.SetPosition(prevPosition + disp);
                controller.onGround = true;
            } else {
                controller.onGround =
                    physics->MoveController(lock, controller.pxController, dtSinceLastFrame, GlmVec3ToPxVec3(disp));
            }
            auto newPosition = PxExtendedVec3ToGlmVec3P(controller.pxController->getPosition());
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
        ecs::Lock<ecs::Read<ecs::HumanController>,
                  ecs::Write<ecs::PhysicsState, ecs::Transform, ecs::InteractController>> lock,
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
                                            double dt,
                                            glm::vec2 dCursor) {
        auto &interact = entity.Get<ecs::InteractController>(lock);
        if (interact.target) {
            auto rotation = glm::vec3(dCursor.y, dCursor.x, 0) * (float)(CVarCursorSensitivity.Get() * 0.1 * dt);
            physics->RotateConstraint(entity, interact.target, GlmVec3ToPxVec3(rotation));
            return true;
        }
        return false;
    }
} // namespace sp
