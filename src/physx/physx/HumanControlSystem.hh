#pragma once

#include "ecs/Ecs.hh"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>

namespace sp {
    class PhysxManager;

    class HumanControlSystem {
    public:
        HumanControlSystem(PhysxManager *physics) : physics(physics) {}
        ~HumanControlSystem() {}

        /**
         * Call this once per frame
         */
        void Frame(double dtSinceLastFrame);

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

        PhysxManager *physics = nullptr;
    };
} // namespace sp
