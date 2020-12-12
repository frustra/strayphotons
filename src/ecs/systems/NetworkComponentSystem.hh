#pragma once

#include <ecs/Ecs.hh>

namespace ecs {
    class NetworkComponentSystem {
    public:
        NetworkComponentSystem(EntityManager &entities);
        ~NetworkComponentSystem();

    private:
        EntityManager &entities;
    };
} // namespace ecs
