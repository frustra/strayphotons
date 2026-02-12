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

    void steamNetConnectionStatusChangedCallback(SteamNetConnectionStatusChangedCallback_t *pInfo) {
        NetworkManager *networkManager = (NetworkManager *)(void *)pInfo->m_info.m_nUserData;
        if (networkManager) networkManager->SteamNetConnectionStatusChanged(pInfo);
    }

    void NetworkManager::SteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *pInfo) {
        char temp[1024];

        // What's the state of the connection?
        switch (pInfo->m_info.m_eState) {
        case k_ESteamNetworkingConnectionState_None:
            // NOTE: We will get callbacks here when we destroy connections.  You can ignore these.
            break;

        case k_ESteamNetworkingConnectionState_ClosedByPeer:
        case k_ESteamNetworkingConnectionState_ProblemDetectedLocally: {
            // Ignore if they were not previously connected.  (If they disconnected
            // before we accepted the connection.)
            if (pInfo->m_eOldState == k_ESteamNetworkingConnectionState_Connected) {

                // Locate the client.  Note that it should have been found, because this
                // is the only codepath where we remove clients (except on shutdown),
                // and connection change callbacks are dispatched in queue order.
                auto itClient = clientsMap.find(pInfo->m_hConn);
                assert(itClient != clientsMap.end());

                // Select appropriate log messages
                const char *pszDebugLogAction;
                if (pInfo->m_info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally) {
                    pszDebugLogAction = "problem detected locally";
                    sprintf(temp,
                        "Alas, %s hath fallen into shadow.  (%s)",
                        itClient->second.m_sNick.c_str(),
                        pInfo->m_info.m_szEndDebug);
                } else {
                    // Note that here we could check the reason code to see if
                    // it was a "usual" connection or an "unusual" one.
                    pszDebugLogAction = "closed by peer";
                    sprintf(temp, "%s hath departed", itClient->second.m_sNick.c_str());
                }

                // Spew something to our own log.  Note that because we put their nick
                // as the connection description, it will show up, along with their
                // transport-specific data (e.g. their IP address)
                Logf("Connection %s %s, reason %d: %s\n",
                    pInfo->m_info.m_szConnectionDescription,
                    pszDebugLogAction,
                    pInfo->m_info.m_eEndReason,
                    pInfo->m_info.m_szEndDebug);

                clientsMap.erase(itClient);

                // Send a message so everybody else knows what happened
                SendStringToAllClients(temp);
            } else {
                assert(pInfo->m_eOldState == k_ESteamNetworkingConnectionState_Connecting);
            }

            // Clean up the connection.  This is important!
            // The connection is "closed" in the network sense, but
            // it has not been destroyed.  We must close it on our end, too
            // to finish up.  The reason information do not matter in this case,
            // and we cannot linger because it's already closed on the other end,
            // so we just pass 0's.
            sockets->CloseConnection(pInfo->m_hConn, 0, nullptr, false);
            break;
        }

        case k_ESteamNetworkingConnectionState_Connecting: {
            // This must be a new connection
            assert(clientsMap.find(pInfo->m_hConn) == clientsMap.end());

            Logf("Connection request from %s", pInfo->m_info.m_szConnectionDescription);

            // A client is attempting to connect
            // Try to accept the connection.
            if (sockets->AcceptConnection(pInfo->m_hConn) != k_EResultOK) {
                // This could fail.  If the remote host tried to connect, but then
                // disconnected, the connection may already be half closed.  Just
                // destroy whatever we have on our side.
                sockets->CloseConnection(pInfo->m_hConn, 0, nullptr, false);
                Logf("Can't accept connection.  (It was already closed?)");
                break;
            }

            // Assign the poll group
            if (!sockets->SetConnectionPollGroup(pInfo->m_hConn, pollGroup)) {
                sockets->CloseConnection(pInfo->m_hConn, 0, nullptr, false);
                Logf("Failed to set poll group?");
                break;
            }

            // Generate a random nick.  A random temporary nick
            // is really dumb and not how you would write a real chat server.
            // You would want them to have some sort of signon message,
            // and you would keep their client in a state of limbo (connected,
            // but not logged on) until them.  I'm trying to keep this example
            // code really simple.
            char nick[64];
            sprintf(nick, "BraveWarrior%d", 10000 + (rand() % 100000));

            // Send them a welcome message
            sprintf(temp,
                "Welcome, stranger.  Thou art known to us for now as '%s'; upon thine command '/nick' we shall know "
                "thee otherwise.",
                nick);
            SendStringToClient(pInfo->m_hConn, temp);

            // Also send them a list of everybody who is already connected
            if (clientsMap.empty()) {
                SendStringToClient(pInfo->m_hConn, "Thou art utterly alone.");
            } else {
                sprintf(temp, "%d companions greet you:", (int)clientsMap.size());
                for (auto &c : clientsMap)
                    SendStringToClient(pInfo->m_hConn, c.second.m_sNick.c_str());
            }

            // Let everybody else know who they are for now
            sprintf(temp, "Hark!  A stranger hath joined this merry host.  For now we shall call them '%s'", nick);
            SendStringToAllClients(temp, pInfo->m_hConn);

            // Add them to the client list
            clientsMap.emplace(pInfo->m_hConn, Client{});

            clientsMap[pInfo->m_hConn].m_sNick = nick;
            // Set the connection name, too, which is useful for debugging
            sockets->SetConnectionName(pInfo->m_hConn, nick);
            break;
        }

        case k_ESteamNetworkingConnectionState_Connected:
            // We will get a callback immediately after accepting the connection.
            // Since we are the server, we can ignore this, it's not news to us.
            break;

        default:
            // Silences -Wswitch
            break;
        }
    }

    void NetworkManager::SendStringToClient(HSteamNetConnection conn, const char *str) {
        sockets->SendMessageToConnection(conn, str, (uint32)strlen(str), k_nSteamNetworkingSend_Reliable, nullptr);
    }

    void NetworkManager::SendStringToAllClients(const char *str, HSteamNetConnection except) {
        for (auto &c : clientsMap) {
            if (c.first != except) SendStringToClient(c.first, str);
        }
    }

    bool NetworkManager::ThreadInit() {
        ZoneScoped;

        // Init GameNetworkingSockets the same for both client and server
        SteamDatagramErrMsg errMsg;
        SteamNetworkingIdentity identity = {};
        bool success = GameNetworkingSockets_Init(nullptr, errMsg);
        if (!success) {
            Errorf("Network init failed: %s", errMsg);
            return false;
        }
        SteamNetworkingUtils()->SetDebugOutputFunction(k_ESteamNetworkingSocketsDebugOutputType_Msg, debugOutput);
        SteamNetworkingUtils()->InitRelayNetworkAccess();
        SteamNetworkingUtils()->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_LogLevel_P2PRendezvous,
            k_ESteamNetworkingSocketsDebugOutputType_Verbose);
        sockets = SteamNetworkingSockets();
        if (!sockets) {
            Errorf("SteamNetworkingSockets return null");
            return false;
        }
        success = sockets->GetIdentity(&identity);
        if (!success) {
            Errorf("SteamNetworkingSockets GetIdentity failed");
            return false;
        }
        SteamNetworkingConfigValue_t opt;
        opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
            (void *)steamNetConnectionStatusChangedCallback);
        listenSocket = sockets->CreateListenSocketP2P(0, 1, &opt);
        pollGroup = sockets->CreatePollGroup();
        return true;
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

        ISteamNetworkingMessage *incomingMsg = nullptr;
        int numMsgs = sockets->ReceiveMessagesOnPollGroup(pollGroup, &incomingMsg, 1);
        if (numMsgs == 0) return;
        if (numMsgs < 0) {
            Errorf("Error checking for messages");
            Shutdown(false);
            return;
        }
        Assertf(numMsgs == 1 && incomingMsg, "ReceiveMessagesOnPollGroup returned invalid message count: %d", numMsgs);
        auto itClient = clientsMap.find(incomingMsg->m_conn);
        assert(itClient != clientsMap.end());

        // '\0'-terminate it to make it easier to parse
        std::string sCmd;
        sCmd.assign((const char *)incomingMsg->m_pData, incomingMsg->m_cbSize);
        const char *cmd = sCmd.c_str();

        // We don't need this anymore.
        incomingMsg->Release();

        // Check for known commands.  None of this example code is secure or robust.
        // Don't write a real server like this, please.

        char temp[1024];
        if (strncmp(cmd, "/nick", 5) == 0) {
            const char *nick = cmd + 5;
            while (isspace(*nick))
                ++nick;

            // Let everybody else know they changed their name
            sprintf(temp, "%s shall henceforth be known as %s", itClient->second.m_sNick.c_str(), nick);
            SendStringToAllClients(temp, itClient->first);

            // Respond to client
            sprintf(temp, "Ye shall henceforth be known as %s", nick);
            SendStringToClient(itClient->first, temp);

            // Actually change their name
            clientsMap[itClient->first].m_sNick = nick;
            // Set the connection name, too, which is useful for debugging
            sockets->SetConnectionName(itClient->first, nick);
            return;
        }

        // Assume it's just a ordinary chat message, dispatch to everybody else
        sprintf(temp, "%s: %s", itClient->second.m_sNick.c_str(), cmd);
        SendStringToAllClients(temp, itClient->first);
    }
} // namespace sp
