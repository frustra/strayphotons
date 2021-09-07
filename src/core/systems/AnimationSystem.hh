#pragma once

#include "ecs/Ecs.hh"

namespace sp {
    class AnimationSystem {
    public:
        AnimationSystem() {}

        bool Frame(double dtSinceLastFrame);
    };
} // namespace sp
