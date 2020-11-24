#pragma once

#include <ecs/Ecs.hh>

namespace ecs {
    class DoorSystem {
    public:
        DoorSystem(ecs::ECS &ecs);
        bool Frame(float dtSinceLastFrame);

    private:
        ecs::ECS &ecs;
    };
} // namespace ecs
