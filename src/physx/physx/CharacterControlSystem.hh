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

        void Frame();

    private:
        PhysxManager &manager;
        ecs::ComponentObserver<ecs::CharacterController> characterControllerObserver;
    };
} // namespace sp
