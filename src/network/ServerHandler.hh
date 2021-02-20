#pragma once

#include <atomic>
#include <ecs/Ecs.hh>
#include <ecs/components/Network.hh>
#include <kissnet.hpp>
#include <network/util/BufferedSocket.hh>
#include <vector>

namespace sp {
    class Game;

    namespace network {
        struct Peer {
            kissnet::tcp_socket socket;
            BufferedSocketInput input;
            BufferedSocketOutput output;

            Peer(kissnet::tcp_socket &&sock) : socket(std::move(sock)), input(socket), output(socket) {}
        };

        class ServerHandler {
        public:
            ServerHandler(Game *game);

            void StartServer(std::string args);
            void StopServer();

            void UpdateEntity(ecs::Lock<ecs::ReadNetworkCompoenents> lock,
                              Tecs::Entity e,
                              ecs::Network &network,
                              bool create = false);

            void ListenerThread();
            void WriterThread();

        private:
            Game *game;
            ecs::ECS &ecs;

            std::atomic_bool running = false;
            std::thread listenerThread;
            std::thread writerThread;

            kissnet::tcp_socket server;
            std::vector<Peer> peers;

            ecs::Observer<ecs::Added<ecs::Network>> networkAddition;
            ecs::Observer<ecs::Removed<ecs::Network>> networkRemoval;

            CFuncCollection funcs;
        };
    } // namespace network
} // namespace sp
