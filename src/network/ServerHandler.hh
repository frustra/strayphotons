#pragma once

#include <ecs/Ecs.hh>
#include <ecs/components/Network.hh>
#include <vector>
#include <atomic>
#include <kissnet.hpp>

namespace sp {
    class Game;

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

        std::atomic_bool listening = false;
        std::thread listenerThread;
        std::thread writerThread;

        kissnet::tcp_socket server;
        std::vector<kissnet::tcp_socket> peers;

        ecs::Observer<ecs::Added<ecs::Network>> networkAddition;
        ecs::Observer<ecs::Removed<ecs::Network>> networkRemoval;

        CFuncCollection funcs;
    };
} // namespace sp
