#include "ServerHandler.hh"

#include <Common.hh>
#include <core/Game.hh>
#include <core/Logging.hh>
#include <ecs/Components.hh>
#include <ecs/EcsImpl.hh>
#include <glm/gtx/string_cast.hpp>
#include <google/protobuf/util/delimited_message_util.h>
#include <kissnet.hpp>
#include <network/protocol/Protocol.pb.h>
#include <picojson/picojson.h>

namespace sp {
    namespace network {
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
            bool previous = running.exchange(true);
            if (!previous) {
                listenerThread = std::thread(&ServerHandler::ListenerThread, this);
                writerThread = std::thread(&ServerHandler::WriterThread, this);
            } else {
                auto endpoint = server.get_bind_loc();
                Errorf("Server is already running at: %s:%d", endpoint.address, endpoint.port);
            }
        }

        void ServerHandler::StopServer() {
            bool previous = running.exchange(false);
            if (previous) {
                Logf("Stopping server...");
                if (writerThread.joinable()) writerThread.join();
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
            // msg["_action"] = picojson::value(create ? "create" : "update");
            // msg["_id"] = picojson::value((int64_t)e.id);
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
                        Assert(ecs::Component<ecs::Physics>::Save(msg["physics"], physics),
                               "Failed to serialize Physics");
                        networkComponent.lastUpdate = physics;
                        networkComponent.initialized = true;
                        updated = true;
                    }
                }
            }
            if (updated) {
                auto str = picojson::value(msg).serialize();
                for (auto &peer : peers) {
                    // std::array<zmq::const_buffer, 2> send_msgs = {
                    //     zmq::buffer(peer),
                    //     zmq::buffer(str)
                    // };
                    // if (!zmq::send_multipart(server, send_msgs)) {
                    //     Errorf("Server failed to send message");
                    // }
                    Message msg;
                    msg.set_action(create ? Message_Action_CREATE_ENTITY : Message_Action_UPDATE_ENTITY);
                    msg.set_id(e.id);
                    msg.set_data(str);
                    google::protobuf::util::SerializeDelimitedToZeroCopyStream(msg, &peer.output);
                }
            }
        }

        /*void ServerHandler::ReceiveMessage() {
            zmq::message_t msg;
            auto result = client.recv(msg, zmq::recv_flags::dontwait);
            if (result) {
                picojson::value root;
                string err = picojson::parse(root, msg.to_string());
                if (!err.empty()) {
                    Logf("%s", msg.to_string());
                    Errorf(err);
                } else {
                    auto ent = root.get<picojson::object>();
                    
                    if (ent.count("_name")) {
                        Tecs::Entity entity = lock.NewEntity();
                        auto name = ent["_name"].get<string>();
                        entity.Set<ecs::Name>(lock, name);
                        if (namedEntities.count(name) != 0) { throw std::runtime_error("Duplicate entity name: " + name); }
                        namedEntities.emplace(name, entity);
                    }
                    for (auto param : ent) {
                        if (param.first == "fov") {
                            view.fov = glm::radians(param.second.get<double>());
                        } else {
                            if (param.first == "extents") {
                                view.extents = sp::MakeVec2(param.second);
                            } else if (param.first == "clip") {
                                view.clip = sp::MakeVec2(param.second);
                            } else if (param.first == "offset") {
                                view.offset = sp::MakeVec2(param.second);
                            } else if (param.first == "clear") {
                                view.clearColor = glm::vec4(sp::MakeVec3(param.second), 1.0f);
                            } else if (param.first == "sky") {
                                view.skyIlluminance = param.second.get<double>();
                            }
                        }
                    }
                    auto autoExecList = root.get<picojson::object>()["autoexec"];
                    if (autoExecList.is<picojson::array>()) {
                        for (auto value : autoExecList.get<picojson::array>()) {
                            auto line = value.get<string>();
                            scene->autoExecList.push_back(line);
                        }
                    }
                }
            }
        }*/

        void ServerHandler::ListenerThread() {
            server = kissnet::tcp_socket({"127.0.0.1", 8000});
            server.bind();
            server.listen();
            server.set_non_blocking(true);

            while (running && server) {
                auto client = server.accept();
                if (client) {
                    auto &peer = peers.emplace_back(std::move(client));

                    Message msg;
                    msg.set_action(Message_Action_PING);
                    msg.set_id(42);
                    msg.set_data("Hello World");
                    google::protobuf::util::SerializeDelimitedToZeroCopyStream(msg, &peer.output);
                }

                for (auto &[socket, input, output] : peers) {
                    output.FlushBuffer();
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
            Logf("Disconnecting peers...");
            for (auto &[socket, input, output] : peers) {
                // Flush remaining buffer
                output.Close();
                while (output.FlushBuffer())
                    ;
                // Close the TCP socket
                socket.close();
            }
            peers.clear();
            server.close();
            Logf("Server listener shutdown.");
        }

        void ServerHandler::WriterThread() {
            while (running && server) {
                {
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
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            Logf("Server writer shutdown.");
        }
    }; // namespace network
}; // namespace sp
