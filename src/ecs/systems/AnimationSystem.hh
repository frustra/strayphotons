#pragma once

#include "physx/PhysxManager.hh"

#include <ecs/Ecs.hh>

namespace ecs {
    class AnimationSystem {
    public:
        AnimationSystem(ecs::ECS &ecs);

        ~AnimationSystem();

        bool Frame(float dtSinceLastFrame);

    private:
        ecs::ECS &ecs;
    };
} // namespace ecs
