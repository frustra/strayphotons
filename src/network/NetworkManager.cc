#include "NetworkManager.hh"

#include <Common.hh>
#include <ecs/EcsImpl.hh>
#include <glm/gtx/string_cast.hpp>
#include <iostream>
#include <network/protocol/Protocol.pb.h>

namespace sp {
    NetworkManager::NetworkManager(ecs::ECS &ecs) : ecs(ecs) {
        auto lock = ecs.StartTransaction<ecs::AddRemove>();
        networkAddition = lock.Watch<ecs::Added<ecs::Network>>();
    }

    void NetworkManager::UpdateEntity(ecs::Lock<ecs::ReadNetworkCompoenents> lock,
                                      Tecs::Entity e,
                                      ecs::Network &network) {
        for (auto &networkComponent : network.components) {
            if (networkComponent.component->name == "renderable" && e.Has<ecs::Renderable>(lock)) {
                auto &renderable = e.Get<ecs::Renderable>(lock);
                if (!networkComponent.initialized ||
                    std::get<ecs::Renderable>(networkComponent.lastUpdate) != renderable) {
                    // Transmit renderable
                    std::cout << "Renderable: " << renderable.model->name << std::endl;
                    networkComponent.lastUpdate = renderable;
                    networkComponent.initialized = true;
                }
            } else if (networkComponent.component->name == "transform" && e.Has<ecs::Transform>(lock)) {
                auto &transform = e.Get<ecs::Transform>(lock);
                if (!networkComponent.initialized ||
                    std::get<ecs::Transform>(networkComponent.lastUpdate) != transform) {
                    // Transmit transform
                    std::cout << "Transform: " << glm::to_string(transform.GetPosition()) << std::endl;
                    networkComponent.lastUpdate = transform;
                    networkComponent.initialized = true;
                }
            } else if (networkComponent.component->name == "physics" && e.Has<ecs::Physics>(lock)) {
                auto &physics = e.Get<ecs::Physics>(lock);
                if (!networkComponent.initialized || std::get<ecs::Physics>(networkComponent.lastUpdate) != physics) {
                    // Transmit physics
                    std::cout << "Physics: " << physics.actor << std::endl;
                    networkComponent.lastUpdate = physics;
                    networkComponent.initialized = true;
                }
            }
        }
    }

    bool NetworkManager::Frame() {
        auto lock = ecs.StartTransaction<ecs::ReadNetworkCompoenents, ecs::Write<ecs::Network>>();
        ecs::Added<ecs::Network> addedNetwork;
        while (networkAddition.Poll(lock, addedNetwork)) {
            auto &e = addedNetwork.entity;
            std::cout << "Entity Added: " << e.id << std::endl;
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
