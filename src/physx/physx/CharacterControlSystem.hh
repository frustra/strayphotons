#pragma once

#include "ecs/Ecs.hh"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>

namespace sp {
    class PhysxManager;

    class CharacterControlSystem {
    public:
        CharacterControlSystem(PhysxManager &manager) : manager(manager) {}
        ~CharacterControlSystem() {}

        void Frame(double dtSinceLastFrame);

        void UpdateController(ecs::Lock<ecs::Read<ecs::Transform>, ecs::Write<ecs::CharacterController>> lock,
                              Tecs::Entity &e);

    private:
        void MoveEntity(ecs::Lock<ecs::Write<ecs::Transform, ecs::CharacterController>> lock,
                        Tecs::Entity entity,
                        double dtSinceLastFrame,
                        glm::vec3 velocity);

        PhysxManager &manager;
    };
} // namespace sp
