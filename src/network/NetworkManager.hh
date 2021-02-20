#pragma once

#include <ecs/Ecs.hh>
#include <ecs/components/Network.hh>
#include <kissnet.hpp>
#include <network/ServerHandler.hh>
#include <vector>

namespace sp {
    class Game;

    namespace network {
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
    }; // namespace network
} // namespace sp
