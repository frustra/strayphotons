#pragma once

#include "ecs/Ecs.hh"

namespace sp {
    class InputManager;

    class AnimationSystem {
    public:
        AnimationSystem() {}

        bool Frame(float dtSinceLastFrame);
    };
} // namespace sp
