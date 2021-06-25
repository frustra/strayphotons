#pragma once

#include "ecs/Ecs.hh"
#include "input/InputManager.hh"
#include "physx/PhysxManager.hh"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>

namespace sp {
    static const std::string INPUT_ACTION_PLAYER_MOVE_FORWARD = INPUT_ACTION_PLAYER_BASE + "/move_forward";
    static const std::string INPUT_ACTION_PLAYER_MOVE_BACKWARD = INPUT_ACTION_PLAYER_BASE + "/move_backward";
    static const std::string INPUT_ACTION_PLAYER_MOVE_LEFT = INPUT_ACTION_PLAYER_BASE + "/move_left";
    static const std::string INPUT_ACTION_PLAYER_MOVE_RIGHT = INPUT_ACTION_PLAYER_BASE + "/move_right";
    static const std::string INPUT_ACTION_PLAYER_MOVE_JUMP = INPUT_ACTION_PLAYER_BASE + "/jump";
    static const std::string INPUT_ACTION_PLAYER_MOVE_CROUCH = INPUT_ACTION_PLAYER_BASE + "/crouch";
    static const std::string INPUT_ACTION_PLAYER_MOVE_SPRINT = INPUT_ACTION_PLAYER_BASE + "/sprint";
    static const std::string INPUT_ACTION_PLAYER_INTERACT = INPUT_ACTION_PLAYER_BASE + "/interact";
    static const std::string INPUT_ACTION_PLAYER_INTERACT_ROTATE = INPUT_ACTION_PLAYER_BASE + "/interact_rotate";

    class HumanControlSystem {
    public:
        HumanControlSystem(ecs::EntityManager *entities, InputManager *input, PhysxManager *physics);
        ~HumanControlSystem();

        /**
         * Call this once per frame
         */
        bool Frame(double dtSinceLastFrame);

        /**
         * Assigns a default HumanController to the given entity.
         */
        ecs::HumanController &AssignController(ecs::Lock<ecs::AddRemove> lock, Tecs::Entity entity, PhysxManager &px);

        /**
         * Teleports the entity and properly syncs to physx.
         */
        void Teleport(ecs::Lock<ecs::Write<ecs::Transform, ecs::HumanController>> lock,
                      Tecs::Entity entity,
                      glm::vec3 position);
        void Teleport(ecs::Lock<ecs::Write<ecs::Transform, ecs::HumanController>> lock,
                      Tecs::Entity entity,
                      glm::vec3 position,
                      glm::quat rotation);

        /**
         * Pick up the object that the player is looking at and make it move at to a fixed location relative to camera
         */
        void Interact(ecs::Entity entity);

    private:
        glm::vec3 CalculatePlayerVelocity(ecs::Entity entity,
                                          double dtSinceLastFrame,
                                          glm::vec3 inDirection,
                                          bool jump,
                                          bool sprint,
                                          bool crouch);
        void MoveEntity(ecs::Entity entity, double dtSinceLastFrame, glm::vec3 velocity);

        /**
         * Resize entity used for crouching and uncrouching. Can perform overlap checks to make sure resize is valid
         */
        bool ResizeEntity(ecs::Entity entity, float height, bool overlapCheck);

        /**
         * Rotate the object the player is currently holding, using mouse input.
         * Returns true if there is currently a target.
         */
        bool InteractRotate(ecs::Entity entity, double dt, glm::vec2 dCursor);

        ecs::EntityManager *entities = nullptr;
        InputManager *input = nullptr;
        PhysxManager *physics = nullptr;
    };
} // namespace sp
