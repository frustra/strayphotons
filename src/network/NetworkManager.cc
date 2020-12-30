#include "NetworkManager.hh"

#include <Common.hh>
#include <core/Game.hh>
#include <core/Logging.hh>
#include <ecs/Components.hh>
#include <ecs/EcsImpl.hh>
#include <glm/gtx/string_cast.hpp>
#include <network/protocol/Protocol.pb.h>
#include <picojson/picojson.h>
#include <zmq_addon.hpp>

namespace sp {
    NetworkManager::NetworkManager(Game *game) : game(game), ecs(game->entityManager.tecs) {
        auto lock = ecs.StartTransaction<ecs::AddRemove>();
        networkAddition = lock.Watch<ecs::Added<ecs::Network>>();
        networkRemoval = lock.Watch<ecs::Removed<ecs::Network>>();

        funcs.Register(this,
                       "startserver",
                       "Start listening for connections (startserver <ip> <port>)",
                       &NetworkManager::StartServer);
        funcs.Register(this, "stopserver", "Stop listening for connections", &NetworkManager::StopServer);
        funcs.Register(this, "connect", "Connect to a server (connect <ip> <port>)", &NetworkManager::Connect);
        funcs.Register(this, "disconnect", "Stop listening for connections", &NetworkManager::Disconnect);
    }

    void NetworkManager::StartServer(std::string args) {
        server = zmq::socket_t(ctx, zmq::socket_type::router);
        server.bind("tcp://127.0.0.1:8000");
    }

    void NetworkManager::StopServer() {
        if (server) { server.close(); }
    }

    void NetworkManager::Connect(std::string args) {
        client = zmq::socket_t(ctx, zmq::socket_type::dealer);
        client.connect("tcp://127.0.0.1:8000");
        
        client.send(zmq::str_buffer("HELLO"));
    }

    void NetworkManager::Disconnect() {
        if (client) { client.close(); }
    }

    void NetworkManager::UpdateEntity(ecs::Lock<ecs::ReadNetworkCompoenents> lock,
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
        if (server && updated) {
            auto str = picojson::value(msg).serialize();
            for (auto &peer : peers) {
                std::array<zmq::const_buffer, 2> send_msgs = {
                    zmq::buffer(peer),
                    zmq::buffer(str)
                };
                if (!zmq::send_multipart(server, send_msgs)) {
                    Errorf("Server failed to send message");
                }
            }
        }
    }

    bool NetworkManager::Frame() {
        if (server) {
            std::vector<zmq::message_t> recv_msgs;
            const auto ret = zmq::recv_multipart(server, std::back_inserter(recv_msgs), zmq::recv_flags::dontwait);
            if (ret && recv_msgs.size() > 1) {
                peers.emplace_back(recv_msgs[0].to_string());
                Logf("Server received: %s", recv_msgs[1].to_string());
            }
            try {
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
            } catch (zmq::error_t &err) {
                Errorf("Network error (server): %s", err.what());
                server.close();
            }
        }
        if (client) {
            try {
                zmq::message_t msg;
                auto result = client.recv(msg, zmq::recv_flags::dontwait);
                if (result) {
                    Logf("%s", msg.to_string());
                }
            } catch (zmq::error_t &err) {
                Errorf("Network error (client): %s", err.what());
                client.close();
            }
        }
        return true;
    }
}; // namespace sp
