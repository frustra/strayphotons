/*
# Stray Photons - Copyright (C) 2026 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/RegisteredThread.hh"
#include "ecs/Ecs.hh"

class ISteamNetworkingSockets;
struct SteamNetConnectionStatusChangedCallback_t;

namespace sp {
    class NetworkManager : public RegisteredThread {
    public:
        NetworkManager();
        ~NetworkManager();

    protected:
        bool ThreadInit() override;
        void ThreadShutdown() override;
        void Frame() override;

    private:
        void Shutdown(bool waitForExit);
        void SteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *pInfo);
        void SendStringToClient(uint32_t conn, const char *str);
        void SendStringToAllClients(const char *str, uint32_t except = 0);

        struct Client {
            std::string m_sNick;
        };

        ISteamNetworkingSockets *sockets = nullptr;
        uint32_t listenSocket, pollGroup;
        std::map<uint32_t, Client> clientsMap;

        ecs::ComponentModifiedObserver<ecs::Network> networkObserver;

        DispatchQueue networkQueue;

        friend void steamNetConnectionStatusChangedCallback(SteamNetConnectionStatusChangedCallback_t *);
    };
} // namespace sp
