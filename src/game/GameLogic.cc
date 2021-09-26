#include "GameLogic.hh"

#include "Game.hh"
#include "assets/AssetManager.hh"
#include "assets/Scene.hh"
#include "assets/Script.hh"
#include "core/CVar.hh"
#include "core/Console.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

#ifdef SP_INPUT_SUPPORT
    #include "input/core/BindingNames.hh"
#endif

#ifdef SP_GRAPHICS_SUPPORT
    #include "graphics/core/GraphicsContext.hh"
#endif

#include <atomic>
#include <chrono>
#include <cmath>
#include <cxxopts.hpp>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/transform.hpp>
#include <sstream>
#include <string>
#include <thread>

namespace sp {
    GameLogic::GameLogic(Game *game) : game(game) {
        funcs.Register(this, "loadscene", "Load a scene", &GameLogic::LoadScene);
        funcs.Register(this, "reloadscene", "Reload current scene", &GameLogic::ReloadScene);
        funcs.Register(this, "printdebug", "Print some debug info about the scene", &GameLogic::PrintDebug);
        funcs.Register(this, "printfocus", "Print the current focus lock state", &GameLogic::PrintFocus);
        funcs.Register(this, "acquirefocus", "Acquire focus for the specified layer", &GameLogic::AcquireFocus);
        funcs.Register(this, "releasefocus", "Release focus for the specified layer", &GameLogic::ReleaseFocus);
        funcs.Register(this,
                       "setsignal",
                       "Set a signal value (setsignal <entity>.<signal> <value>)",
                       &GameLogic::SetSignal);
        funcs.Register(this,
                       "clearsignal",
                       "Clear a signal value (clearsignal <entity>.<signal>)",
                       &GameLogic::ClearSignal);
        funcs.Register(this, "tracetecs", "Save an ECS performance trace (tracetecs <time_ms>)", &GameLogic::TraceTecs);
    }

    GameLogic::~GameLogic() {}

#ifdef SP_INPUT_SUPPORT
    void GameLogic::HandleInput() {
        bool spawnDebug = false;
        {
            auto lock = ecs::World.StartTransaction<ecs::Write<ecs::EventInput>>();

            ecs::Event event;
            if (player.Has<ecs::EventInput>(lock)) {
                if (ecs::EventInput::Poll(lock, player, INPUT_EVENT_SPAWN_DEBUG, event)) { spawnDebug = true; }
            }
        }

        if (spawnDebug) { // Spawn dodecahedron
            auto lock = ecs::World.StartTransaction<ecs::AddRemove>();
            auto entity = lock.NewEntity();
            entity.Set<ecs::Owner>(lock, ecs::Owner::SystemId::GAME_LOGIC);
            auto model = GAssets.LoadModel("dodecahedron");
            entity.Set<ecs::Renderable>(lock, model);
            entity.Set<ecs::Transform>(lock, glm::vec3(0, 5, 0));

    #ifdef SP_PHYSICS_SUPPORT_PHYSX
            entity.Set<ecs::Physics>(lock, model, true);
    #endif
        }
    }
#endif

    bool GameLogic::Frame(double dtSinceLastFrame) {
#ifdef SP_INPUT_SUPPORT
        HandleInput();
#endif

        if (!scene) return true;

        {
            auto lock = ecs::World.StartTransaction<ecs::WriteAll>();
            for (auto &entity : lock.EntitiesWith<ecs::Script>()) {
                auto &script = entity.Get<ecs::Script>(lock);
                script.OnTick(lock, entity, dtSinceLastFrame);
            }

            for (auto &entity : lock.EntitiesWith<ecs::TriggerArea>()) {
                auto &area = entity.Get<ecs::TriggerArea>(lock);

                if (!area.triggered) {
                    for (auto triggerableEntity : lock.EntitiesWith<ecs::Triggerable>()) {
                        auto &transform = triggerableEntity.Get<ecs::Transform>(lock);
                        auto entityPos = transform.GetPosition();
                        if (entityPos.x > area.boundsMin.x && entityPos.y > area.boundsMin.y &&
                            entityPos.z > area.boundsMin.z && entityPos.x < area.boundsMax.x &&
                            entityPos.y < area.boundsMax.y && entityPos.z < area.boundsMax.z) {
                            Debugf("Entity at: %f %f %f", entityPos.x, entityPos.y, entityPos.z);
                            Logf("Triggering event: %s", area.command);
                            area.triggered = true;
                            GetConsoleManager().QueueParseAndExecute(area.command);
                            break;
                        }
                    }
                }
            }
        }

        if (!scene) return true;

        return true;
    }

    void GameLogic::LoadPlayer() {
        {
            auto lock = ecs::World.StartTransaction<ecs::AddRemove>();
            ecs::DestroyAllWith<ecs::Owner>(lock, ecs::Owner(ecs::Owner::OwnerType::PLAYER, 0));
        }

        if (playerScene != nullptr) {
            for (auto &line : playerScene->unloadExecList) {
                GetConsoleManager().ParseAndExecute(line);
            }
        }

        playerScene.reset();
        player = Tecs::Entity();

        {
            auto lock = ecs::World.StartTransaction<ecs::AddRemove>();
            playerScene = GAssets.LoadScene("player", lock, ecs::Owner(ecs::Owner::OwnerType::PLAYER, 0));

            for (auto e : lock.EntitiesWith<ecs::Name>()) {
                if (e.Has<ecs::Owner>(lock)) {
                    auto &name = e.Get<ecs::Name>(lock);
                    auto &owner = e.Get<ecs::Owner>(lock);
                    if (owner == ecs::Owner(ecs::Owner::OwnerType::PLAYER, 0)) {
                        if (name == "player") {
                            player = e;
                            break;
                        }
                    }
                }
            }
            Assert(!!player, "Player scene doesn't contain an entity named player");

            auto spawn = ecs::EntityWith<ecs::Name>(lock, "player-spawn");
            if (spawn.Has<ecs::Transform>(lock)) {
                auto &spawnTransform = spawn.Get<ecs::Transform>(lock);
                player.Set<ecs::Transform>(lock, spawnTransform);
            } else if (!player.Has<ecs::Transform>(lock)) {
                player.Set<ecs::Transform>(lock, glm::vec3(0));
            }

            player.Set<ecs::HumanController>(lock);
#ifdef SP_PHYSICS_SUPPORT_PHYSX
            auto &interact = player.Set<ecs::InteractController>(lock);
            interact.manager = &game->physics;
#endif

            // Mark the player as being able to activate trigger areas
            player.Set<ecs::Triggerable>(lock);

#ifdef SP_GRAPHICS_SUPPORT
            game->graphics.GetContext()->AttachView(player);
#endif

            if (playerScene) {
                for (auto &line : playerScene->autoExecList) {
                    GetConsoleManager().ParseAndExecute(line);
                }
            }
        }
    }

    void GameLogic::LoadScene(std::string sceneName) {
#ifdef SP_GRAPHICS_SUPPORT
        game->graphics.RenderLoading();
#endif
        {
            auto lock = ecs::World.StartTransaction<ecs::AddRemove>();
            ecs::DestroyAllWith<ecs::Owner>(lock, ecs::Owner(ecs::Owner::OwnerType::SCENE, 0));
        }

        if (scene != nullptr) {
            for (auto &line : scene->unloadExecList) {
                GetConsoleManager().ParseAndExecute(line);
            }
        }

        scene.reset();
        {
            auto lock = ecs::World.StartTransaction<ecs::AddRemove>();
            scene = GAssets.LoadScene(sceneName, lock, ecs::Owner(ecs::Owner::OwnerType::SCENE, 0));

            for (auto e : lock.EntitiesWith<ecs::Name>()) {
                auto &name = e.Get<ecs::Name>(lock);
                if (name == "player" && e != player) {
                    name = "player-spawn";
                    if (e.Has<ecs::Transform>(lock)) {
                        auto &spawnTransform = e.Get<ecs::Transform>(lock);
                        auto &playerTransform = player.Get<ecs::Transform>(lock);
                        playerTransform.SetPosition(spawnTransform.GetPosition());
                        playerTransform.SetRotation(spawnTransform.GetRotation());
                        playerTransform.UpdateCachedTransform(lock);
                    }
                    break;
                }
            }
        }

        if (scene) {
            for (auto &line : scene->autoExecList) {
                GetConsoleManager().ParseAndExecute(line);
            }
        }
    }

    void GameLogic::ReloadScene(std::string arg) {
        if (scene) {
            if (arg == "reset") {
                LoadPlayer();
                LoadScene(scene->name);
            } else {
                // TODO: Fix this to not respawn the player
                LoadScene(scene->name);
            }
        }
    }

    void GameLogic::PrintDebug() {
        Logf("Currently loaded scene: %s", scene ? scene->name : "none");
        if (!scene) return;
        auto lock = ecs::World.StartTransaction<ecs::Read<ecs::Name,
                                                          ecs::Transform,
                                                          ecs::HumanController,
                                                          ecs::LightSensor,
                                                          ecs::SignalOutput,
                                                          ecs::EventInput,
                                                          ecs::FocusLayer,
                                                          ecs::FocusLock>>();
        if (player && player.Has<ecs::Transform, ecs::HumanController>(lock)) {
            auto &transform = player.Get<ecs::Transform>(lock);
            auto &controller = player.Get<ecs::HumanController>(lock);
            auto position = transform.GetPosition();
#ifdef SP_PHYSICS_SUPPORT_PHYSX
            auto pxFeet = controller.pxController->getFootPosition();
            Logf("Player position: [%f, %f, %f], feet: %f", position.x, position.y, position.z, pxFeet.y);
#else
            Logf("Player position: [%f, %f, %f]", position.x, position.y, position.z);
#endif
            Logf("Player velocity: [%f, %f, %f]", controller.velocity.x, controller.velocity.y, controller.velocity.z);
            Logf("Player on ground: %s", controller.onGround ? "true" : "false");
        } else {
            Logf("Scene has no valid player");
        }

        for (auto ent : lock.EntitiesWith<ecs::LightSensor>()) {
            auto &sensor = ent.Get<ecs::LightSensor>(lock);
            auto &i = sensor.illuminance;

            Logf("Light sensor %s: %f %f %f", ecs::ToString(lock, ent), i.r, i.g, i.b);
        }

        for (auto ent : lock.EntitiesWith<ecs::SignalOutput>()) {
            auto &output = ent.Get<ecs::SignalOutput>(lock);

            Logf("Signal output %s:", ecs::ToString(lock, ent));
            for (auto &[signalName, value] : output.GetSignals()) {
                Logf("  %s: %.2f", signalName, value);
            }
        }

        auto &focusLock = lock.Get<ecs::FocusLock>();
        for (auto ent : lock.EntitiesWith<ecs::EventInput>()) {
            auto &input = ent.Get<ecs::EventInput>(lock);

            if (ent.Has<ecs::FocusLayer>(lock)) {
                auto &layer = ent.Get<ecs::FocusLayer>(lock);
                std::stringstream ss;
                ss << layer;
                if (focusLock.HasPrimaryFocus(layer)) {
                    Logf("Event input %s: (has primary focus: %s)", ecs::ToString(lock, ent), ss.str());
                } else if (focusLock.HasFocus(layer)) {
                    Logf("Event input %s: (has focus: %s)", ecs::ToString(lock, ent), ss.str());
                } else {
                    Logf("Event input %s: (no focus: %s)", ecs::ToString(lock, ent), ss.str());
                }
            } else {
                Logf("Event input %s: (no focus layer)", ecs::ToString(lock, ent));
            }
            for (auto &[eventName, queue] : input.events) {
                if (queue.empty()) {
                    Logf("  %s: empty", eventName);
                } else {
                    Logf("  %s: %u events", eventName, queue.size());
                }
            }
        }
    }

    void GameLogic::PrintFocus() {
        auto lock = ecs::World.StartTransaction<ecs::Read<ecs::FocusLock>>();

        if (lock.Has<ecs::FocusLock>()) {
            auto &focusLock = lock.Get<ecs::FocusLock>();
            std::stringstream ss;
            ss << "Active focus layers: " << focusLock;
            Logf(ss.str());
        } else {
            Errorf("World does not have a FocusLock");
        }
    }

    void GameLogic::AcquireFocus(std::string args) {
        std::stringstream stream(args);
        ecs::FocusLayer layer;
        stream >> layer;
        if (layer != ecs::FocusLayer::NEVER && layer != ecs::FocusLayer::ALWAYS) {
            auto lock = ecs::World.StartTransaction<ecs::Write<ecs::FocusLock>>();

            if (lock.Has<ecs::FocusLock>()) {
                if (!lock.Get<ecs::FocusLock>().AcquireFocus(layer)) {
                    std::stringstream ss;
                    ss << layer;
                    Logf("Failed to acquire focus layer: %s", ss.str());
                }
            } else {
                Errorf("World does not have a FocusLock");
            }
        }
    }

    void GameLogic::ReleaseFocus(std::string args) {
        std::stringstream stream(args);
        ecs::FocusLayer layer;
        stream >> layer;
        if (layer != ecs::FocusLayer::NEVER && layer != ecs::FocusLayer::ALWAYS) {
            auto lock = ecs::World.StartTransaction<ecs::Write<ecs::FocusLock>>();

            if (lock.Has<ecs::FocusLock>()) {
                lock.Get<ecs::FocusLock>().ReleaseFocus(layer);
            } else {
                Errorf("World does not have a FocusLock");
            }
        }
    }

    std::tuple<std::string, std::string> parseSignal(std::string name) {
        size_t delimiter = name.find_last_of('.');
        std::string entityName = name;
        std::string signalName = "value";
        if (delimiter != std::string::npos) {
            entityName = name.substr(0, delimiter);
            signalName = name.substr(delimiter + 1);
        }
        return std::make_tuple(entityName, signalName);
    }

    void GameLogic::SetSignal(std::string args) {
        std::stringstream stream(args);
        std::string signalStr;
        double value;
        stream >> signalStr;
        stream >> value;

        auto [entityName, signalName] = parseSignal(signalStr);

        auto lock = ecs::World.StartTransaction<ecs::Read<ecs::Name>, ecs::Write<ecs::SignalOutput>>();
        auto entity = ecs::EntityWith<ecs::Name>(lock, entityName);
        if (!entity) {
            Logf("Signal entity %s not found", entityName);
            return;
        }
        if (!entity.Has<ecs::SignalOutput>(lock)) {
            Logf("%s is not a signal output", entityName);
            return;
        }

        auto &signalComp = entity.Get<ecs::SignalOutput>(lock);
        signalComp.SetSignal(signalName, value);
    }

    void GameLogic::ClearSignal(std::string args) {
        std::stringstream stream(args);
        std::string signalStr;
        stream >> signalStr;

        auto [entName, signalName] = parseSignal(signalStr);

        auto lock = ecs::World.StartTransaction<ecs::Read<ecs::Name>, ecs::Write<ecs::SignalOutput>>();
        auto entity = ecs::EntityWith<ecs::Name>(lock, entName);
        if (entity && entity.Has<ecs::SignalOutput>(lock)) {
            auto &signalComp = entity.Get<ecs::SignalOutput>(lock);
            signalComp.ClearSignal(signalName);
        }
    }

    void GameLogic::TraceTecs(std::string args) {
        std::stringstream stream(args);
        size_t timeMs = 0;
        stream >> timeMs;

        if (timeMs == 0) {
            Logf("Trace time must be specified in milliseconds.");
            return;
        }

        static std::atomic_bool tracing(false);
        bool current = tracing;
        if (current || !tracing.compare_exchange_strong(current, true)) {
            Logf("A performance trace is already in progress");
            return;
        }

        std::map<std::thread::id, std::string> threadNames;
        threadNames[std::this_thread::get_id()] = "Main";
        for (auto registeredThread : GetRegisteredThreads()) {
            threadNames[registeredThread->GetThreadId()] = registeredThread->threadName;
        }

        std::thread thread([timeMs, threadNames] {
            ecs::World.StartTrace();
            std::this_thread::sleep_for(std::chrono::milliseconds(timeMs));
            auto trace = ecs::World.StopTrace();

            for (auto &[id, name] : threadNames) {
                trace.SetThreadName(name, id);
            }

            std::ofstream traceFile("tecs-trace.csv");
            trace.SaveToCSV(traceFile);
            traceFile.close();

            Logf("Tecs performance trace complete");
            tracing = false;
        });
        thread.detach();
    }
} // namespace sp
