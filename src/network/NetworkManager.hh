#pragma once

#include <ecs/Ecs.hh>
#include <ecs/components/Network.hh>
#include <vector>
#include <kissnet.hpp>
#include <network/ServerHandler.hh>

namespace sp {
    class Game;

    class NetworkManager {
    public:
        NetworkManager(Game *game);

        void Connect(std::string args);
        void Disconnect();

        bool Frame();

    private:
        Game *game;
        ecs::ECS &ecs;

        ServerHandler server;

        std::vector<kissnet::tcp_socket> clients;

        CFuncCollection funcs;
    };
} // namespace sp
