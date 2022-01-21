#pragma once

#include "ecs/Ecs.hh"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>

namespace sp {
    class PhysxManager;

    class HumanControlSystem {
    public:
        HumanControlSystem(PhysxManager &manager) : manager(manager) {}
        ~HumanControlSystem() {}

        /**
         * Call this once per frame
         */
        void Frame();

    private:
        void UpdatePlayerVelocity(ecs::Lock<ecs::Read<ecs::Transform>, ecs::Write<ecs::HumanController>> lock,
            Tecs::Entity entity,
            glm::vec3 inDirection,
            bool jump,
            bool sprint,
            bool crouch);
        void MoveEntity(ecs::Lock<ecs::Write<ecs::Transform, ecs::HumanController>> lock, Tecs::Entity entity);

        PhysxManager &manager;
    };
} // namespace sp
