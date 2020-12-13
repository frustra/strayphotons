#pragma once

#include <ecs/Ecs.hh>
#include <ecs/components/Network.hh>

namespace sp {
    class NetworkManager {
    public:
        NetworkManager(ecs::ECS &ecs);

        void UpdateEntity(ecs::Lock<ecs::ReadNetworkCompoenents> lock, Tecs::Entity e, ecs::Network &network);

        bool Frame();

    private:
        ecs::ECS &ecs;

        ecs::Observer<ecs::Added<ecs::Network>> networkAddition;
    };
} // namespace sp
