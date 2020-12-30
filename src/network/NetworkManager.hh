#pragma once

#include <ecs/Ecs.hh>
#include <ecs/components/Network.hh>
#include <zmq.hpp>
#include <vector>

namespace sp {
    class Game;

    class NetworkManager {
    public:
        NetworkManager(Game *game);

        void StartServer(std::string args);
        void StopServer();
        void Connect(std::string args);
        void Disconnect();

        void UpdateEntity(ecs::Lock<ecs::ReadNetworkCompoenents> lock,
                          Tecs::Entity e,
                          ecs::Network &network,
                          bool create = false);

        bool Frame();

    private:
        Game *game;
        ecs::ECS &ecs;

        zmq::context_t ctx;
        zmq::socket_t server;
        zmq::socket_t client;
        std::vector<std::string> peers;

        ecs::Observer<ecs::Added<ecs::Network>> networkAddition;
        ecs::Observer<ecs::Removed<ecs::Network>> networkRemoval;

        CFuncCollection funcs;
    };
} // namespace sp
