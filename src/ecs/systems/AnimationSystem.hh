#pragma once

#include "physx/PhysxManager.hh"

#include <ecs/Ecs.hh>

namespace sp {
    class InputManager;
}

namespace ecs {
    class AnimationSystem {
    public:
        AnimationSystem(EntityManager &entities);

        ~AnimationSystem();

        bool Frame(float dtSinceLastFrame);

    private:
        EntityManager &entities;
    };
} // namespace ecs
