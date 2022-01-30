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
    // static CVar<float> CVarCharacterUpdateRate("p.CharacterUpdateRate", 0.3,
    //     "Character view update frequency (seconds)");
    static CVar<bool> CVarPropJumping("x.PropJumping", false, "Disable player collision with held object");

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
                if (event.component.pxController) {
                    auto userData = event.component.pxController->getUserData();
                    if (userData) {
                        delete (CharacterControllerUserData *)userData;
                        event.component.pxController->setUserData(nullptr);
                    }

                    event.component.pxController->release();
                }
            }
        }

        for (Tecs::Entity entity : lock.EntitiesWith<ecs::CharacterController>()) {
            if (!entity.Has<ecs::CharacterController, ecs::Transform>(lock)) continue;

            auto &controller = entity.Get<ecs::CharacterController>(lock);
            if (!controller.pxController) continue;
            auto &transform = entity.Get<ecs::Transform>(lock);

            auto userData = (CharacterControllerUserData *)controller.pxController->getUserData();

            float targetHeight = ecs::PLAYER_CAPSULE_HEIGHT;
            glm::vec3 targetPosition = transform.GetPosition();

            auto target = controller.target.Get(lock);
            if (target && target.Has<ecs::Transform>(lock)) {
                auto &targetTransform = target.Get<ecs::Transform>(lock);
                targetPosition = targetTransform.GetGlobalTransform(lock).GetPosition();
                auto originPosition = transform.GetGlobalTransform(lock).GetPosition();
                targetHeight = std::max(0.1f,
                    targetPosition.y - originPosition.y - controller.pxController->getRadius());
                targetPosition.y = originPosition.y;
            }

            // If the origin moved, teleport the controller
            if (transform.HasChanged(userData->actorData.transformChangeNumber)) {
                controller.pxController->setHeight(targetHeight);
                controller.pxController->setFootPosition(GlmVec3ToPxExtendedVec3(targetPosition));

                userData->onGround = false;
                userData->velocity = glm::vec3(0);
                // userData->deltaSinceUpdate = glm::vec3(0);
                userData->actorData.transformChangeNumber = transform.ChangeNumber();
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

            // Calculate new character velocity
            glm::vec3 lateralMovement = glm::vec3(0);
            lateralMovement.x = ecs::SignalBindings::GetSignal(lock, entity, INPUT_SIGNAL_MOVE_WORLD_X);
            lateralMovement.z = ecs::SignalBindings::GetSignal(lock, entity, INPUT_SIGNAL_MOVE_WORLD_Z);
            float verticalMovement = ecs::SignalBindings::GetSignal(lock, entity, INPUT_SIGNAL_MOVE_WORLD_Y);
            bool sprint = ecs::SignalBindings::GetSignal(lock, entity, INPUT_SIGNAL_MOVE_SPRINT) >= 0.5;
            bool noclip = ecs::SignalBindings::GetSignal(lock, entity, INPUT_SIGNAL_MOVE_NOCLIP) >= 0.5;

            float speed = sprint ? CVarCharacterSprintSpeed.Get() : CVarCharacterMovementSpeed.Get();

            if (lateralMovement != glm::vec3(0)) lateralMovement = glm::normalize(lateralMovement) * speed;
            verticalMovement = std::clamp(verticalMovement, -1.0f, 1.0f) * speed;

            if (noclip) {
                userData->velocity.x = lateralMovement.x;
                userData->velocity.y = verticalMovement;
                userData->velocity.z = lateralMovement.z;
            } else if (userData->onGround) {
                userData->velocity.x = lateralMovement.x;
                if (verticalMovement > 0.5) {
                    userData->velocity.y = ecs::PLAYER_JUMP_VELOCITY;
                } else {
                    // Always try moving down so that onGround detection is more consistent.
                    userData->velocity.y = -ecs::PLAYER_GRAVITY * (float)(manager.interval.count() / 1e9);
                }
                userData->velocity.z = lateralMovement.z;
            } else {
                userData->velocity += lateralMovement * ecs::PLAYER_AIR_STRAFE *
                                      (float)(manager.interval.count() / 1e9);
                userData->velocity.y -= ecs::PLAYER_GRAVITY * (float)(manager.interval.count() / 1e9);
            }

            if (userData->noclipping != noclip) {
                auto actor = controller.pxController->getActor();
                manager.SetCollisionGroup(actor, noclip ? ecs::PhysicsGroup::NoClip : ecs::PhysicsGroup::Player);
                userData->noclipping = noclip;
            }

            PxFilterData data;
            if (noclip) {
                data.word0 = ecs::PHYSICS_GROUP_NOCLIP;
            } else if (CVarPropJumping.Get()) {
                data.word0 = ecs::PHYSICS_GROUP_WORLD | ecs::PHYSICS_GROUP_PLAYER;
            } else {
                data.word0 = ecs::PHYSICS_GROUP_WORLD;
            }
            PxControllerFilters moveQueryFilter(&data);

            // Update the capsule position to match target
            auto targetDelta = targetPosition /* + userData->deltaSinceUpdate*/ -
                               PxExtendedVec3ToGlmVec3(controller.pxController->getFootPosition());

            auto moveResult = controller.pxController->move(GlmVec3ToPxVec3(targetDelta),
                0,
                manager.interval.count() / 1e9,
                moveQueryFilter);
            userData->onGround = moveResult & PxControllerCollisionFlag::eCOLLISION_DOWN;

            // Move controller based on velocity and update ECS transform
            auto disp = userData->velocity * (float)(manager.interval.count() / 1e9);
            auto prevPosition = controller.pxController->getFootPosition();
            moveResult = controller.pxController->move(GlmVec3ToPxVec3(disp),
                0,
                manager.interval.count() / 1e9,
                moveQueryFilter);
            userData->onGround = moveResult & PxControllerCollisionFlag::eCOLLISION_DOWN;
            auto newPosition = controller.pxController->getFootPosition();
            auto deltaPos = PxVec3ToGlmVec3(newPosition - prevPosition);

            // Update the velocity based on what happened in physx
            auto physxVelocity = deltaPos / (float)(manager.interval.count() / 1e9);
            auto inputSpeed = glm::length(userData->velocity);
            if (glm::length(physxVelocity) > inputSpeed) {
                // Don't allow the physics simulation to accelerate the character faster than the input speed
                userData->velocity = glm::normalize(physxVelocity) * inputSpeed;
            } else {
                userData->velocity = physxVelocity;
            }

            transform.SetPosition(PxExtendedVec3ToGlmVec3(newPosition));
            userData->actorData.transformChangeNumber = transform.ChangeNumber();

            auto movementProxy = controller.movementProxy.Get(lock);
            if (movementProxy.Has<ecs::Transform>(lock)) {
                auto &proxyTransform = movementProxy.Get<ecs::Transform>(lock);
                proxyTransform.Translate(deltaPos);
            }

            // userData->deltaSinceUpdate += deltaPos;

            // auto now = chrono_clock::now();
            // auto nextUpdate = userData->lastUpdate +
            //                   std::chrono::milliseconds((size_t)(CVarCharacterUpdateRate.Get() * 1000.0f));
            // if (lateralMovement == glm::vec3(0) || nextUpdate <= now) {
            // transform.Translate(userData->deltaSinceUpdate);
            // userData->actorData.transformChangeNumber = transform.ChangeNumber();
            // userData->deltaSinceUpdate = glm::vec3(0);
            // userData->lastUpdate = now;
            // }

            // TODO: Temporary physics visualization
            // Tecs::Entity physicsBox = ecs::EntityWith<ecs::Name>(lock, "player.player-physics");
            // if (physicsBox.Has<ecs::Transform>(lock)) {
            //     auto &transform = physicsBox.Get<ecs::Transform>(lock);
            //     transform.SetScale(glm::vec3(0.1f, controller.pxController->getHeight(), 0.1f));
            //     transform.SetPosition(PxExtendedVec3ToGlmVec3(controller.pxController->getPosition()));
            // }
        }
    }
} // namespace sp
