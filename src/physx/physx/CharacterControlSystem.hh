#pragma once

#include "ecs/Ecs.hh"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>

namespace sp {
    class PhysxManager;

    class CharacterControlSystem {
    public:
        CharacterControlSystem(PhysxManager &manager);
        ~CharacterControlSystem() {}

        void Frame(double dtSinceLastFrame);

    private:
        PhysxManager &manager;
        ecs::Observer<ecs::ComponentEvent<ecs::CharacterController>> characterControllerObserver;
    };
} // namespace sp
