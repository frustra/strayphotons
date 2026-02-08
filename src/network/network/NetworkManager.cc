/*
# Stray Photons - Copyright (C) 2026 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "NetworkManager.hh"

// #include "assets/AssetManager.hh"
// #include "common/Tracing.hh"
// #include "console/CVar.hh"
#include "ecs/EcsImpl.hh"
// #include "game/GameEntities.hh"

#include <steam/isteamnetworkingmessages.h>

namespace sp {
    NetworkManager::NetworkManager()
        : RegisteredThread("NetworkManager", std::chrono::milliseconds(20), false), networkQueue("NetworkQueue") {

        {
            auto lock = ecs::StartTransaction<ecs::AddRemove>();
            networkObserver = lock.Watch<ecs::ComponentModifiedEvent<ecs::Network>>();
        }

        StartThread();
    }

    bool NetworkManager::ThreadInit() {
        ZoneScoped;

        // Start listening on ports / connect to server
        auto *gameServerNetworking = SteamGameServerNetworkingMessages();
        if (gameServerNetworking) {
            gameServerNetworking->
        }
        return true;
    }

    void NetworkManager::Shutdown(bool waitForExit) {
        StopThread(waitForExit);
    }

    NetworkManager::~NetworkManager() {
        Shutdown(true);
    }

    void NetworkManager::Frame() {
        ZoneScoped;
        auto lock = ecs::StartTransaction<ecs::Read<ecs::Network>>();

        ecs::ComponentModifiedEvent<ecs::Network> networkChangedEvent;
        while (networkObserver.Poll(lock, networkChangedEvent)) {
            if (!networkChangedEvent.Has<ecs::EventInput, ecs::Network>(lock)) continue;

            // auto &network = networkChangedEvent.Get<ecs::Network>(lock);
        }
    }
} // namespace sp
