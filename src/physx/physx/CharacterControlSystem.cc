#include "CharacterControlSystem.hh"

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
    static CVar<float> CVarControllerMovementSpeed("p.ControllerMovementSpeed",
                                                   3.0,
                                                   "Character controller movement speed (m/s)");

    void CharacterControlSystem::Frame(double dtSinceLastFrame) {
        auto lock = ecs::World.StartTransaction<
            ecs::Read<ecs::Name, ecs::SignalOutput, ecs::SignalBindings, ecs::FocusLayer, ecs::FocusLock>,
            ecs::Write<ecs::EventInput, ecs::Transform, ecs::CharacterController, ecs::InteractController>>();

        for (Tecs::Entity entity : lock.EntitiesWith<ecs::CharacterController>()) {
            if (!entity.Has<ecs::Transform>(lock)) continue;

            auto &controller = entity.Get<ecs::CharacterController>(lock);
            if (!controller.pxController) continue;

            glm::vec3 inputMovement = glm::vec3(0);
            inputMovement.z -= ecs::SignalBindings::GetSignal(lock, entity, INPUT_SIGNAL_MOVE_FORWARD);
            inputMovement.z += ecs::SignalBindings::GetSignal(lock, entity, INPUT_SIGNAL_MOVE_BACK);
            inputMovement.x -= ecs::SignalBindings::GetSignal(lock, entity, INPUT_SIGNAL_MOVE_LEFT);
            inputMovement.x += ecs::SignalBindings::GetSignal(lock, entity, INPUT_SIGNAL_MOVE_RIGHT);
            inputMovement.x = std::clamp(inputMovement.x, -1.0f, 1.0f);
            inputMovement.z = std::clamp(inputMovement.z, -1.0f, 1.0f);

            glm::vec3 movement = inputMovement;
            Tecs::Entity inputParent = controller.inputParent.Get(lock);
            if (inputParent.Has<ecs::Transform>(lock)) {
                auto &parentTransform = inputParent.Get<ecs::Transform>(lock);
                auto parentRotation = parentTransform.GetGlobalRotation(lock);
                movement = parentRotation * inputMovement;
                if (std::abs(movement.y) > 0.999) { movement = parentRotation * glm::vec3(0, -movement.y, 0); }
                movement.y = 0;
            }

            if (movement != glm::vec3(0)) {
                float speed = CVarControllerMovementSpeed.Get();
                movement = glm::normalize(movement) * speed;
            }

            if (controller.onGround) {
                controller.velocity.x = movement.x;
                controller.velocity.y -= 0.01; // Always try moving down so that onGround detection is more consistent.
                controller.velocity.z = movement.z;
            } else {
                controller.velocity += movement * ecs::PLAYER_AIR_STRAFE * (float)dtSinceLastFrame;
                controller.velocity.y -= ecs::PLAYER_GRAVITY * dtSinceLastFrame;
            }

            MoveEntity(lock, entity, dtSinceLastFrame, controller.velocity);
        }
    }

    void CharacterControlSystem::UpdateController(
        ecs::Lock<ecs::Read<ecs::Transform>, ecs::Write<ecs::CharacterController>> lock,
        Tecs::Entity &e) {
        if (!e.Has<ecs::CharacterController, ecs::Transform>(lock)) return;

        auto &controller = e.Get<ecs::CharacterController>(lock);
        auto &transform = e.Get<ecs::Transform>(lock);

        auto position = transform.GetGlobalPosition(lock);
        // Offset the capsule position so the transform origin is at the botttom
        auto pxPosition = GlmVec3ToPxExtendedVec3(position + glm::vec3(0, ecs::PLAYER_CAPSULE_HEIGHT / 2, 0));

        if (!controller.pxController) {
            // Capsule controller description will want to be data driven
            physx::PxCapsuleControllerDesc desc;
            desc.position = pxPosition;
            desc.upDirection = physx::PxVec3(0, 1, 0);
            desc.radius = ecs::PLAYER_RADIUS;
            desc.height = ecs::PLAYER_CAPSULE_HEIGHT;
            desc.density = 0.5f;
            desc.material = manager.pxPhysics->createMaterial(0.3f, 0.3f, 0.3f);
            desc.climbingMode = physx::PxCapsuleClimbingMode::eCONSTRAINED;
            desc.reportCallback = new ControllerHitReport(); // TODO: Does this have to be free'd?
            desc.userData = new CharacterControllerUserData(transform.ChangeNumber());

            auto pxController = manager.controllerManager->createController(desc);
            Assert(pxController->getType() == physx::PxControllerShapeType::eCAPSULE,
                   "Physx did not create a valid PxCapsuleController");

            pxController->setStepOffset(ecs::PLAYER_STEP_HEIGHT);

            physx::PxShape *shape;
            pxController->getActor()->getShapes(&shape, 1);
            physx::PxFilterData data;
            data.word0 = PhysxCollisionGroup::PLAYER;
            shape->setQueryFilterData(data);
            shape->setSimulationFilterData(data);

            controller.pxController = static_cast<physx::PxCapsuleController *>(pxController);
        }

        auto userData = (CharacterControllerUserData *)controller.pxController->getUserData();
        if (transform.HasChanged(userData->transformChangeNumber)) {
            controller.pxController->setPosition(pxPosition);
            userData->transformChangeNumber = transform.ChangeNumber();
        }
    }

    void CharacterControlSystem::MoveEntity(ecs::Lock<ecs::Write<ecs::Transform, ecs::CharacterController>> lock,
                                            Tecs::Entity entity,
                                            double dtSinceLastFrame,
                                            glm::vec3 velocity) {
        auto &transform = entity.Get<ecs::Transform>(lock);
        auto &controller = entity.Get<ecs::CharacterController>(lock);

        if (controller.pxController) {
            auto userData = (CharacterControllerUserData *)controller.pxController->getUserData();
            if (transform.HasChanged(userData->transformChangeNumber)) return;

            auto disp = velocity * (float)dtSinceLastFrame;
            auto prevPosition = PxExtendedVec3ToGlmVec3P(controller.pxController->getPosition());
            glm::vec3 newPosition;
            controller.onGround = manager.MoveController(controller.pxController,
                                                         dtSinceLastFrame,
                                                         GlmVec3ToPxVec3(disp));
            newPosition = PxExtendedVec3ToGlmVec3P(controller.pxController->getPosition());

            // Don't accelerate more than our current velocity
            auto velocityPosition = glm::min(prevPosition + glm::abs(disp), newPosition);
            velocityPosition = glm::max(prevPosition - glm::abs(disp), velocityPosition);

            // Update the velocity based on what happened in physx
            controller.velocity = (velocityPosition - prevPosition) / (float)dtSinceLastFrame;
            userData->velocity = controller.velocity;

            // Offset the capsule position so the transform origin is at the bottom
            float capsuleHeight = controller.pxController->getHeight();
            transform.SetPosition(newPosition - glm::vec3(0, capsuleHeight / 2, 0));
            userData->transformChangeNumber = transform.ChangeNumber();
        }
    }
} // namespace sp
