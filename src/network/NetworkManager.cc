#include "NetworkManager.hh"

#include <Common.hh>
#include <core/Game.hh>
#include <core/Logging.hh>
#include <ecs/Components.hh>
#include <ecs/EcsImpl.hh>
#include <glm/gtx/string_cast.hpp>
#include <network/protocol/Protocol.pb.h>
#include <picojson/picojson.h>

namespace sp {
    namespace network {
        NetworkManager::NetworkManager(Game *game) : game(game), ecs(game->entityManager.tecs), server(game) {
            funcs.Register(this, "connect", "Connect to a server (connect <ip> <port>)", &NetworkManager::Connect);
            funcs.Register(this, "disconnect", "Stop listening for connections", &NetworkManager::Disconnect);
        }

        void NetworkManager::Connect(std::string args) {
            // client = zmq::socket_t(ctx, zmq::socket_type::dealer);
            // client.connect("tcp://127.0.0.1:8000");

            // client.send(zmq::str_buffer("HELLO"));
        }

        void NetworkManager::Disconnect() {
            // if (client) { client.close(); }
        }

        bool NetworkManager::Frame() {
            // for (auto &client : clients) {
            //     zmq::message_t msg;
            //     auto result = client.recv(msg, zmq::recv_flags::dontwait);
            //     if (result) {
            //         Logf("%s", msg.to_string());
            //     }
            // }
            return true;
        }
    }; // namespace network
}; // namespace sp
