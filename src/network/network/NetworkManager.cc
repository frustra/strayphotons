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
// #include "game/GameEntities.hh"
#include "common/InlineString.hh"
#include "ecs/EcsImpl.hh"

#include <steam/isteamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#include <steam/steamnetworkingsockets.h>

namespace sp {
    NetworkManager::NetworkManager()
        : RegisteredThread("NetworkManager", std::chrono::milliseconds(20), false), networkQueue("NetworkQueue") {

        {
            auto lock = ecs::StartTransaction<ecs::AddRemove>();
            networkObserver = lock.Watch<ecs::ComponentModifiedEvent<ecs::Network>>();
        }

        StartThread();
    }

    void debugOutput(ESteamNetworkingSocketsDebugOutputType eType, const char *pszMsg) {
        Logf("Network: %s %s", eType, pszMsg);
    }

    bool NetworkManager::ThreadInit() {
        ZoneScoped;

        // Init GameNetworkingSockets the same for both client and server
        SteamDatagramErrMsg errMsg;
        SteamNetworkingIdentity identity = {};
        bool success = GameNetworkingSockets_Init(nullptr, errMsg);
        if (success) {
            sockets = SteamNetworkingSockets();
            if (sockets) success = sockets->GetIdentity(&identity);
        }
        if (success) {
            SteamNetworkingUtils()->SetDebugOutputFunction(k_ESteamNetworkingSocketsDebugOutputType_Msg, debugOutput);
        } else {
            Errorf("Network init failed: %s", errMsg);
        }
        return success;
    }

    void NetworkManager::ThreadShutdown() {
        GameNetworkingSockets_Kill();
        sockets = nullptr;
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
