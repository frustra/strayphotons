#pragma once

#include "ecs/Ecs.hh"
#include "ecs/SignalExpression.hh"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>

namespace sp {
    class PhysxManager;

    class CharacterControlSystem {
    public:
        CharacterControlSystem(PhysxManager &manager);
        ~CharacterControlSystem() {}

        void RegisterEvents();
        void Frame(ecs::Lock<ecs::ReadSignalsLock,
            ecs::Read<ecs::EventInput, ecs::SceneProperties>,
            ecs::Write<ecs::TransformTree, ecs::CharacterController>> lock);

    private:
        PhysxManager &manager;
        ecs::ComponentObserver<ecs::CharacterController> characterControllerObserver;
    };
} // namespace sp
