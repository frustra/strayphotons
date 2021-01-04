#include "ServerHandler.hh"

#include <Common.hh>
#include <core/Game.hh>
#include <core/Logging.hh>
#include <ecs/Components.hh>
#include <ecs/EcsImpl.hh>
#include <glm/gtx/string_cast.hpp>
#include <network/protocol/Protocol.pb.h>
#include <picojson/picojson.h>
#include <kissnet.hpp>

namespace sp {
    ServerHandler::ServerHandler(Game *game) : game(game), ecs(game->entityManager.tecs) {
        auto lock = ecs.StartTransaction<ecs::AddRemove>();
        networkAddition = lock.Watch<ecs::Added<ecs::Network>>();
        networkRemoval = lock.Watch<ecs::Removed<ecs::Network>>();

        funcs.Register(this,
                       "startserver",
                       "Start listening for connections (startserver <ip> <port>)",
                       &ServerHandler::StartServer);
        funcs.Register(this, "stopserver", "Stop listening for connections", &ServerHandler::StopServer);
    }

    void ServerHandler::StartServer(std::string args) {
        bool previous = listening.exchange(true);
        if (!previous) {
            listenerThread = std::thread(&ServerHandler::ListenerThread, this);
        } else {
            auto endpoint = server.get_bind_loc();
            Errorf("Server is already running at: %s:%d", endpoint.address, endpoint.port);
        }
    }

    void ServerHandler::StopServer() {
        bool previous = listening.exchange(false);
        if (previous) {
            if (server) server.close();
            if (listenerThread.joinable()) listenerThread.join();
        } else {
            Errorf("No server is currently running");
        }
    }

    void ServerHandler::UpdateEntity(ecs::Lock<ecs::ReadNetworkCompoenents> lock,
                                      Tecs::Entity e,
                                      ecs::Network &network,
                                      bool create) {
        bool updated = create;
        picojson::object msg;
        msg["_action"] = picojson::value(create ? "create" : "update");
        msg["_id"] = picojson::value((int64_t)e.id);
        if (e.Has<ecs::Name>(lock)) { msg["_name"] = picojson::value(e.Get<ecs::Name>(lock)); }
        for (auto &networkComponent : network.components) {
            if (networkComponent.component->name == "renderable" && e.Has<ecs::Renderable>(lock)) {
                auto &renderable = e.Get<ecs::Renderable>(lock);
                if (create || !networkComponent.initialized ||
                    std::get<ecs::Renderable>(networkComponent.lastUpdate) != renderable) {
                    // Transmit renderable
                    msg["renderable"] = picojson::value();
                    Assert(ecs::Component<ecs::Renderable>::Save(msg["renderable"], renderable),
                           "Failed to serialize Renderable");
                    networkComponent.lastUpdate = renderable;
                    networkComponent.initialized = true;
                    updated = true;
                }
            } else if (networkComponent.component->name == "transform" && e.Has<ecs::Transform>(lock)) {
                auto &transform = e.Get<ecs::Transform>(lock);
                if (create || !networkComponent.initialized ||
                    std::get<ecs::Transform>(networkComponent.lastUpdate) != transform) {
                    // Transmit transform
                    msg["transform"] = picojson::value();
                    Assert(ecs::Component<ecs::Transform>::Save(msg["transform"], transform),
                           "Failed to serialize Transform");
                    networkComponent.lastUpdate = transform;
                    networkComponent.initialized = true;
                    updated = true;
                }
            } else if (networkComponent.component->name == "physics" && e.Has<ecs::Physics>(lock)) {
                auto &physics = e.Get<ecs::Physics>(lock);
                if (create || !networkComponent.initialized ||
                    std::get<ecs::Physics>(networkComponent.lastUpdate) != physics) {
                    // Transmit physics
                    msg["physics"] = picojson::value();
                    Assert(ecs::Component<ecs::Physics>::Save(msg["physics"], physics), "Failed to serialize Physics");
                    networkComponent.lastUpdate = physics;
                    networkComponent.initialized = true;
                    updated = true;
                }
            }
        }
        // if (updated) {
        //     auto str = picojson::value(msg).serialize();
        //     for (auto &peer : peers) {
        //         std::array<zmq::const_buffer, 2> send_msgs = {
        //             zmq::buffer(peer),
        //             zmq::buffer(str)
        //         };
        //         if (!zmq::send_multipart(server, send_msgs)) {
        //             Errorf("Server failed to send message");
        //         }
        //     }
        // }
    }

    void ServerHandler::ListenerThread() {
        server = kissnet::tcp_socket({ "127.0.0.1", 8000 });
        server.bind();
        server.listen();

        while (listening && server) {
            auto client = server.accept();
            if (client) {
                std::string msg = "Hello World\r\n";
                client.send(reinterpret_cast<const std::byte*>(msg.c_str()), msg.size());
                client.close();
            }
        }
    }

    void ServerHandler::WriterThread() {
        if (server) {
            auto lock = ecs.StartTransaction<ecs::ReadNetworkCompoenents, ecs::Write<ecs::Network>>();
            ecs::Added<ecs::Network> addedNetwork;
            while (networkAddition.Poll(lock, addedNetwork)) {
                auto &e = addedNetwork.entity;
                if (e.Has<ecs::Network>(lock)) {
                    auto &network = e.Get<ecs::Network>(lock);
                    UpdateEntity(lock, e, network, true);
                }
            }
            for (auto e : lock.EntitiesWith<ecs::Network>()) {
                auto &network = e.Get<ecs::Network>(lock);
                UpdateEntity(lock, e, network);
            }
        }
    }
}; // namespace sp
