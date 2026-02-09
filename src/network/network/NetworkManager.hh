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

        ISteamNetworkingSockets *sockets;

        ecs::ComponentModifiedObserver<ecs::Network> networkObserver;

        DispatchQueue networkQueue;
    };
} // namespace sp
