#pragma once

#include "ecs/Ecs.hh"
#include "input/InputManager.hh"

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

    class PhysxManager;

    class HumanControlSystem {
    public:
        HumanControlSystem(InputManager *input, PhysxManager *physics);
        ~HumanControlSystem();

        /**
         * Call this once per frame
         */
        bool Frame(double dtSinceLastFrame);

        /**
         * Pick up the object that the player is looking at and make it move at to a fixed location relative to camera
         */
        void Interact(ecs::Lock<ecs::Read<ecs::HumanController>,
                                ecs::Write<ecs::PhysicsState, ecs::Transform, ecs::InteractController>> lock,
                      Tecs::Entity entity);

    private:
        glm::vec3 CalculatePlayerVelocity(ecs::Lock<ecs::Read<ecs::Transform>, ecs::Write<ecs::HumanController>> lock,
                                          Tecs::Entity entity,
                                          double dtSinceLastFrame,
                                          glm::vec3 inDirection,
                                          bool jump,
                                          bool sprint,
                                          bool crouch);
        void MoveEntity(ecs::Lock<ecs::Write<ecs::PhysicsState, ecs::Transform, ecs::HumanController>> lock,
                        Tecs::Entity entity,
                        double dtSinceLastFrame,
                        glm::vec3 velocity);

        /**
         * Rotate the object the player is currently holding, using mouse input.
         * Returns true if there is currently a target.
         */
        bool InteractRotate(ecs::Lock<ecs::Read<ecs::InteractController>> lock,
                            Tecs::Entity entity,
                            double dt,
                            glm::vec2 dCursor);

        InputManager *input = nullptr;
        PhysxManager *physics = nullptr;
    };
} // namespace sp
