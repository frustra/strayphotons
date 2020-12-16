#include "NetworkManager.hh"

#include <Common.hh>
#include <arpa/inet.h>
#include <core/Logging.hh>
#include <ecs/EcsImpl.hh>
#include <glm/gtx/string_cast.hpp>
#include <network/protocol/Protocol.pb.h>

namespace sp {
    NetworkManager::NetworkManager(ecs::ECS &ecs) : ecs(ecs) {
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
        server = zmq::socket_t(ctx, zmq::socket_type::push);
        server.bind("tcp://127.0.0.1:8000");
    }

    void NetworkManager::StopServer() {
        if (server) { server.close(); }
    }
    void NetworkManager::Connect(std::string args) {}
    void NetworkManager::Disconnect() {}

    void NetworkManager::UpdateEntity(ecs::Lock<ecs::ReadNetworkCompoenents> lock,
                                      Tecs::Entity e,
                                      ecs::Network &network) {
        bool updated = false;
        std::stringstream ss;
        ss << "Entity: " << e.id << std::endl;
        for (auto &networkComponent : network.components) {
            if (networkComponent.component->name == "renderable" && e.Has<ecs::Renderable>(lock)) {
                auto &renderable = e.Get<ecs::Renderable>(lock);
                if (!networkComponent.initialized ||
                    std::get<ecs::Renderable>(networkComponent.lastUpdate) != renderable) {
                    // Transmit renderable
                    ss << "Renderable: " << renderable.model->name << std::endl;
                    networkComponent.lastUpdate = renderable;
                    networkComponent.initialized = true;
                    updated = true;
                }
            } else if (networkComponent.component->name == "transform" && e.Has<ecs::Transform>(lock)) {
                auto &transform = e.Get<ecs::Transform>(lock);
                if (!networkComponent.initialized ||
                    std::get<ecs::Transform>(networkComponent.lastUpdate) != transform) {
                    // Transmit transform
                    ss << "Transform: " << glm::to_string(transform.GetPosition()) << std::endl;
                    networkComponent.lastUpdate = transform;
                    networkComponent.initialized = true;
                    updated = true;
                }
            } else if (networkComponent.component->name == "physics" && e.Has<ecs::Physics>(lock)) {
                auto &physics = e.Get<ecs::Physics>(lock);
                if (!networkComponent.initialized || std::get<ecs::Physics>(networkComponent.lastUpdate) != physics) {
                    // Transmit physics
                    ss << "Physics: " << physics.actor << std::endl;
                    networkComponent.lastUpdate = physics;
                    networkComponent.initialized = true;
                    updated = true;
                }
            }
        }
        if (server && updated) { server.send(zmq::buffer(ss.str()), zmq::send_flags::dontwait); }
    }

    bool NetworkManager::Frame() {
        auto lock = ecs.StartTransaction<ecs::ReadNetworkCompoenents, ecs::Write<ecs::Network>>();
        ecs::Added<ecs::Network> addedNetwork;
        while (networkAddition.Poll(lock, addedNetwork)) {
            auto &e = addedNetwork.entity;
            if (server) {
                std::stringstream ss;
                ss << "Entity Added: " << e.id << std::endl;
                server.send(zmq::buffer(ss.str()), zmq::send_flags::dontwait);
            }
            auto &network = e.Get<ecs::Network>(lock);
            UpdateEntity(lock, e, network);
        }
        for (auto e : lock.EntitiesWith<ecs::Network>()) {
            auto &network = e.Get<ecs::Network>(lock);
            UpdateEntity(lock, e, network);
        }
        return true;
    }
}; // namespace sp
