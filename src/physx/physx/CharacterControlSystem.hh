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

        void Frame(
            ecs::Lock<ecs::Read<ecs::Name, ecs::SignalOutput, ecs::SignalBindings, ecs::FocusLayer, ecs::FocusLock>,
                ecs::Write<ecs::Transform, ecs::EventInput, ecs::CharacterController>> lock);

    private:
        PhysxManager &manager;
        ecs::ComponentObserver<ecs::CharacterController> characterControllerObserver;
    };
} // namespace sp
