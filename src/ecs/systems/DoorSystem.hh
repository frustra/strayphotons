#pragma once

#include <ecs/Ecs.hh>

namespace ecs {
    class DoorSystem {
    public:
        DoorSystem(EntityManager &entities);
        bool Frame(float dtSinceLastFrame);

    private:
        EntityManager &entities;
    };
} // namespace ecs
