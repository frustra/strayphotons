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
    static CVar<float> CVarCharacterUpdateRate("p.CharacterUpdateRate",
        0.3,
        "Character view update frequency (seconds)");

    CharacterControlSystem::CharacterControlSystem(PhysxManager &manager) : manager(manager) {
        auto lock = ecs::World.StartTransaction<ecs::AddRemove>();
        characterControllerObserver = lock.Watch<ecs::ComponentEvent<ecs::CharacterController>>();
    }

    void CharacterControlSystem::Frame(
        ecs::Lock<ecs::Read<ecs::Name, ecs::SignalOutput, ecs::SignalBindings, ecs::FocusLayer, ecs::FocusLock>,
            ecs::Write<ecs::Transform, ecs::CharacterController>> lock) {
        // Update PhysX with any added or removed CharacterControllers
        ecs::ComponentEvent<ecs::CharacterController> event;
        while (characterControllerObserver.Poll(lock, event)) {
            if (event.type == Tecs::EventType::ADDED) {
                if (event.entity.Has<ecs::CharacterController>(lock)) {
                    auto &controller = event.entity.Get<ecs::CharacterController>(lock);
                    if (!controller.pxController) {
                        auto characterUserData = new CharacterControllerUserData(event.entity, 0);

                        PxCapsuleControllerDesc desc;
                        desc.position = PxExtendedVec3(0, 0, 0);
                        desc.upDirection = PxVec3(0, 1, 0);
                        desc.radius = ecs::PLAYER_RADIUS;
                        desc.height = ecs::PLAYER_CAPSULE_HEIGHT;
                        desc.density = 0.5f;
                        desc.material = manager.pxPhysics->createMaterial(0.3f, 0.3f, 0.3f);
                        desc.climbingMode = PxCapsuleClimbingMode::eCONSTRAINED;
                        desc.reportCallback = manager.controllerHitReporter.get();
                        desc.userData = characterUserData;

                        auto pxController = manager.controllerManager->createController(desc);
                        Assert(pxController->getType() == PxControllerShapeType::eCAPSULE,
                            "Physx did not create a valid PxCapsuleController");

                        pxController->setStepOffset(ecs::PLAYER_STEP_HEIGHT);
                        auto actor = pxController->getActor();
                        actor->userData = &characterUserData->actorData;

                        manager.SetCollisionGroup(actor, ecs::PhysicsGroup::Player);

                        controller.pxController = static_cast<PxCapsuleController *>(pxController);
                    }
                }
            } else if (event.type == Tecs::EventType::REMOVED) {
                if (event.component.pxController) manager.RemoveController(event.component.pxController);
            }
        }

        for (Tecs::Entity entity : lock.EntitiesWith<ecs::CharacterController>()) {
            if (!entity.Has<ecs::CharacterController, ecs::Transform>(lock)) continue;

            auto &controller = entity.Get<ecs::CharacterController>(lock);
            if (!controller.pxController) continue;

            auto userData = (CharacterControllerUserData *)controller.pxController->getUserData();

            auto target = controller.target.Get(lock);
            if (target && target.Has<ecs::Transform>(lock)) {
                auto &targetTransform = target.Get<ecs::Transform>(lock);
                auto &originTransform = entity.Get<ecs::Transform>(lock);

                auto targetPosition = targetTransform.GetGlobalTransform(lock).GetPosition();
                auto originPosition = originTransform.GetGlobalTransform(lock).GetPosition();
                auto targetHeight = std::max(0.1f,
                    targetPosition.y - originPosition.y - controller.pxController->getRadius());
                targetPosition.y = originPosition.y;

                // If the origin moved, teleport the controller
                if (originTransform.HasChanged(userData->actorData.transformChangeNumber)) {
                    controller.pxController->setHeight(targetHeight);
                    controller.pxController->setFootPosition(GlmVec3ToPxExtendedVec3(targetPosition));

                    userData->onGround = false;
                    userData->velocity = glm::vec3(0);
                    userData->deltaSinceUpdate = glm::vec3(0);
                    userData->actorData.transformChangeNumber = originTransform.ChangeNumber();
                }

                // Update the capsule height
                auto currentHeight = controller.pxController->getHeight();
                if (currentHeight > targetHeight) {
                    controller.pxController->resize(targetHeight);
                } else if (currentHeight < targetHeight) {
                    auto sweepDist = targetHeight - currentHeight;

                    PxSweepBuffer result;
                    manager.SweepQuery(controller.pxController->getActor(), PxVec3(0, 1, 0), sweepDist, result);
                    if (result.getNbAnyHits() > 0) {
                        auto hit = result.getAnyHit(0);
                        controller.pxController->resize(currentHeight + hit.distance);
                    } else {
                        controller.pxController->resize(targetHeight);
                    }
                }

                // Update the capsule position to match target
                auto targetDelta = targetPosition + userData->deltaSinceUpdate -
                                   PxExtendedVec3ToGlmVec3(controller.pxController->getFootPosition());
                userData->onGround = manager.MoveController(controller.pxController,
                    manager.interval.count() / 1e9,
                    GlmVec3ToPxVec3(targetDelta));

                // Calculate new character velocity
                glm::vec3 movement = glm::vec3(0);
                movement.x = ecs::SignalBindings::GetSignal(lock, entity, INPUT_SIGNAL_MOVE_WORLD_X);
                movement.z = ecs::SignalBindings::GetSignal(lock, entity, INPUT_SIGNAL_MOVE_WORLD_Z);

                if (movement != glm::vec3(0)) {
                    float speed = CVarCharacterMovementSpeed.Get();
                    movement = glm::normalize(movement) * speed;
                }

                if (userData->onGround) {
                    userData->velocity.x = movement.x;
                    userData->velocity.y -=
                        0.01; // Always try moving down so that onGround detection is more consistent.
                    userData->velocity.z = movement.z;
                } else {
                    userData->velocity += movement * ecs::PLAYER_AIR_STRAFE * (float)(manager.interval.count() / 1e9);
                    userData->velocity.y -= ecs::PLAYER_GRAVITY * (float)(manager.interval.count() / 1e9);
                }

                // Move controller based on velocity and update ECS transform
                auto disp = userData->velocity * (float)(manager.interval.count() / 1e9);
                auto prevPosition = controller.pxController->getFootPosition();
                userData->onGround = manager.MoveController(controller.pxController,
                    manager.interval.count() / 1e9,
                    GlmVec3ToPxVec3(disp));
                auto deltaPos = PxVec3ToGlmVec3(controller.pxController->getFootPosition() - prevPosition);

                // Update the velocity based on what happened in physx
                auto physxVelocity = deltaPos / (float)(manager.interval.count() / 1e9);
                auto inputSpeed = glm::length(userData->velocity);
                if (glm::length(physxVelocity) > inputSpeed) {
                    // Don't allow the physics simulation to accelerate the character faster than the input speed
                    userData->velocity = glm::normalize(physxVelocity) * inputSpeed;
                } else {
                    userData->velocity = physxVelocity;
                }

                userData->deltaSinceUpdate += deltaPos;

                auto now = chrono_clock::now();
                auto nextUpdate = userData->lastUpdate +
                                  std::chrono::milliseconds((size_t)(CVarCharacterUpdateRate.Get() * 1000.0f));
                if (movement == glm::vec3(0) || nextUpdate <= now) {
                    originTransform.Translate(userData->deltaSinceUpdate);
                    userData->actorData.transformChangeNumber = originTransform.ChangeNumber();
                    userData->deltaSinceUpdate = glm::vec3(0);
                    userData->lastUpdate = now;
                }

                // TODO: Temporary physics visualization
                Tecs::Entity physicsBox = ecs::EntityWith<ecs::Name>(lock, "player.player-physics");
                if (physicsBox.Has<ecs::Transform>(lock)) {
                    auto &transform = physicsBox.Get<ecs::Transform>(lock);
                    transform.SetScale(glm::vec3(0.1f, controller.pxController->getHeight(), 0.1f));
                    transform.SetPosition(PxExtendedVec3ToGlmVec3(controller.pxController->getPosition()));
                }
            }
        }
    }
} // namespace sp
