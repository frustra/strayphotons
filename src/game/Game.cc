#include "Game.hh"

#include "assets/Script.hh"
#include "console/Console.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "core/RegisteredThread.hh"
#include "core/assets/AssetManager.hh"
#include "ecs/Ecs.hh"
#include "ecs/EcsImpl.hh"
#include "game/SceneManager.hh"

#ifdef SP_GRAPHICS_SUPPORT
    #include "graphics/core/GraphicsContext.hh"
#endif

#include <cxxopts.hpp>
#include <glm/glm.hpp>

#if RUST_CXX
    #include <lib.rs.h>
#endif

namespace sp {
    Game::Game(cxxopts::ParseResult &options, Script *startupScript)
        : options(options), startupScript(startupScript)
#ifdef SP_GRAPHICS_SUPPORT
          ,
          graphics(this)
#endif
#ifdef SP_XR_SUPPORT
          ,
          xr(this)
#endif
    {
        funcs.Register(this, "reloadplayer", "Reload player scene", &Game::ReloadPlayer);
        funcs.Register(this, "printdebug", "Print some debug info about the scene", &Game::PrintDebug);
        funcs.Register(this, "printevents", "Print out the current state of event queues", &Game::PrintEvents);
        funcs.Register(this, "printsignals", "Print out the values and bindings of signals", &Game::PrintSignals);
    }

    Game::~Game() {}

    int Game::Start() {
        bool triggeredExit = false;
        int exitCode = 0;

        CFunc<int> cfExit("exit", "Quits the game", [&](int arg) {
            triggeredExit = true;
            exitCode = arg;
        });

        if (options.count("cvar")) {
            for (auto cvarline : options["cvar"].as<vector<string>>()) {
                GetConsoleManager().ParseAndExecute(cvarline);
            }
        }

        Debugf("Bytes of memory used per entity: %u", ecs::World.GetBytesPerEntity());

        {
            auto lock = ecs::World.StartTransaction<ecs::AddRemove>();
            lock.Set<ecs::FocusLock>();
        }

#if RUST_CXX
        sp::rust::print_hello();
#endif

        try {
#ifdef SP_GRAPHICS_SUPPORT
            if (options["headless"].count() == 0) {
                debugGui = std::make_unique<DebugGuiManager>();
                graphics.Init();

                // must create all gui instances after initializing graphics, except for the special debug gui
                menuGui = std::make_unique<MenuGuiManager>(this->graphics);
            }
#endif

#ifdef SP_XR_SUPPORT
            if (options["no-vr"].count() == 0) xr.LoadXrSystem();
#endif

            auto &scenes = GetSceneManager();
            ReloadPlayer();
            if (options.count("map")) {
                scenes.QueueActionAndBlock(SceneAction::LoadScene, options["map"].as<string>());
            }

            if (startupScript != nullptr) {
                startupScript->Exec();
            } else if (!options.count("map")) {
                scenes.QueueActionAndBlock(SceneAction::LoadScene, "menu");
                {
                    auto lock = ecs::World.StartTransaction<ecs::Write<ecs::FocusLock>>();
                    lock.Get<ecs::FocusLock>().AcquireFocus(ecs::FocusLayer::MENU);
                }
            }

            lastFrameTime = chrono_clock::now();

            while (!triggeredExit) {
                if (ShouldStop()) break;
                if (!Frame()) break;
            }
            return exitCode;
        } catch (char const *err) {
            Errorf("%s", err);
            return 1;
        }
    }

    bool Game::Frame() {
        GetConsoleManager().Update(startupScript);

        auto frameTime = chrono_clock::now();
        double dt = (double)(frameTime - lastFrameTime).count();
        dt /= chrono_clock::duration(std::chrono::seconds(1)).count();

#ifdef SP_INPUT_SUPPORT_GLFW
        if (glfwInputHandler) glfwInputHandler->Frame();
#endif

        {
            auto lock = ecs::World.StartTransaction<ecs::WriteAll>();
            for (auto &entity : lock.EntitiesWith<ecs::Script>()) {
                auto &script = entity.Get<ecs::Script>(lock);
                script.OnTick(lock, entity, dt);
            }
        }

#ifdef SP_GRAPHICS_SUPPORT
        if (!graphics.Frame()) return false;
#endif
        if (!animation.Frame(dt)) return false;

        lastFrameTime = frameTime;
        return true;
    }

    bool Game::ShouldStop() {
#ifdef SP_GRAPHICS_SUPPORT
        return !graphics.HasActiveContext();
#else
        return false;
#endif
    }

    void Game::ReloadPlayer() {
        auto &scenes = GetSceneManager();
        player = scenes.LoadPlayer();

#ifdef SP_GRAPHICS_SUPPORT
        auto context = graphics.GetContext();
        if (context) context->AttachView(player);
#endif

        scenes.RespawnPlayer();
        scenes.QueueActionAndBlock(SceneAction::LoadBindings);
    }

    void Game::PrintDebug() {
        auto lock =
            ecs::World.StartTransaction<ecs::Read<ecs::Name, ecs::Transform, ecs::HumanController, ecs::LightSensor>>();
        if (player && player.Has<ecs::Transform, ecs::HumanController>(lock)) {
            auto &transform = player.Get<ecs::Transform>(lock);
            auto position = transform.GetPosition();
#ifdef SP_PHYSICS_SUPPORT_PHYSX
            auto &controller = player.Get<ecs::HumanController>(lock);
            if (controller.pxController) {
                auto pxFeet = controller.pxController->getFootPosition();
                Logf("Player position: [%f, %f, %f], feet: %f", position.x, position.y, position.z, pxFeet.y);
            } else {
                Logf("Player position: [%f, %f, %f]", position.x, position.y, position.z);
            }
            auto userData = (CharacterControllerUserData *)controller.pxController->getUserData();
            Logf("Player velocity: [%f, %f, %f]", userData->velocity.x, userData->velocity.y, userData->velocity.z);
            Logf("Player on ground: %s", userData->onGround ? "true" : "false");
#else
            Logf("Player position: [%f, %f, %f]", position.x, position.y, position.z);
#endif
        } else {
            Logf("Scene has no valid player");
        }

        for (auto ent : lock.EntitiesWith<ecs::LightSensor>()) {
            auto &sensor = ent.Get<ecs::LightSensor>(lock);
            auto &i = sensor.illuminance;

            Logf("Light sensor %s: %f %f %f", ecs::ToString(lock, ent), i.r, i.g, i.b);
        }
    }

    void Game::PrintEvents(std::string entityName) {
        auto lock = ecs::World.StartTransaction<
            ecs::Read<ecs::Name, ecs::EventInput, ecs::EventBindings, ecs::FocusLayer, ecs::FocusLock>>();

        auto &focusLock = lock.Get<ecs::FocusLock>();
        for (auto ent : lock.EntitiesWith<ecs::EventInput>()) {
            if (!entityName.empty()) {
                if (!ent.Has<ecs::Name>(lock) || ent.Get<ecs::Name>(lock) != entityName) continue;
            }

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

            auto &input = ent.Get<ecs::EventInput>(lock);
            for (auto &[eventName, queue] : input.events) {
                if (queue.empty()) {
                    Logf("  %s: empty", eventName);
                } else {
                    Logf("  %s: %u events", eventName, queue.size());
                }
            }
        }

        for (auto ent : lock.EntitiesWith<ecs::EventBindings>()) {
            if (!entityName.empty()) {
                if (!ent.Has<ecs::Name>(lock) || ent.Get<ecs::Name>(lock) != entityName) continue;
            }

            if (ent.Has<ecs::FocusLayer>(lock)) {
                auto &layer = ent.Get<ecs::FocusLayer>(lock);
                std::stringstream ss;
                ss << layer;
                if (focusLock.HasPrimaryFocus(layer)) {
                    Logf("Event binding %s: (has primary focus: %s)", ecs::ToString(lock, ent), ss.str());
                } else if (focusLock.HasFocus(layer)) {
                    Logf("Event binding %s: (has focus: %s)", ecs::ToString(lock, ent), ss.str());
                } else {
                    Logf("Event binding %s: (no focus: %s)", ecs::ToString(lock, ent), ss.str());
                }
            } else {
                Logf("Event binding %s: (no focus layer)", ecs::ToString(lock, ent));
            }

            auto &bindings = ent.Get<ecs::EventBindings>(lock);
            for (auto &bindingName : bindings.GetBindingNames()) {
                auto list = bindings.Lookup(bindingName);
                Logf("    %s:%s", bindingName, list->empty() ? " none" : "");
                for (auto &target : *list) {
                    auto e = target.first.Get(lock);
                    if (e) {
                        Logf("      %s on %s", target.second, ecs::ToString(lock, e));
                    } else {
                        Logf("      %s on %s(missing)", target.second, target.first.Name());
                    }
                }
            }
        }
    }

    void Game::PrintSignals(std::string entityName) {
        auto lock = ecs::World.StartTransaction<
            ecs::Read<ecs::Name, ecs::SignalOutput, ecs::SignalBindings, ecs::FocusLayer, ecs::FocusLock>>();
        Logf("Signal outputs:");
        for (auto ent : lock.EntitiesWith<ecs::SignalOutput>()) {
            if (!entityName.empty()) {
                if (!ent.Has<ecs::Name>(lock) || ent.Get<ecs::Name>(lock) != entityName) continue;
            }

            auto &output = ent.Get<ecs::SignalOutput>(lock);
            auto &signals = output.GetSignals();

            Logf("  %s:%s", ecs::ToString(lock, ent), signals.empty() ? " none" : "");
            for (auto &[signalName, value] : signals) {
                Logf("    %s: %.2f", signalName, value);
            }
        }

        Logf("");
        Logf("Signal bindings:");
        for (auto ent : lock.EntitiesWith<ecs::SignalBindings>()) {
            if (!entityName.empty()) {
                if (!ent.Has<ecs::Name>(lock) || ent.Get<ecs::Name>(lock) != entityName) continue;
            }

            auto &bindings = ent.Get<ecs::SignalBindings>(lock);
            auto bindingNames = bindings.GetBindingNames();
            Logf("  %s:%s", ecs::ToString(lock, ent), bindingNames.empty() ? " none" : "");
            for (auto &bindingName : bindingNames) {
                auto list = bindings.Lookup(bindingName);
                std::stringstream ss;
                ss << bindingName << ": ";
                if (list->sources.empty()) {
                    ss << "none";
                } else {
                    ss << list->operation;
                }
                Logf("    %s", ss.str());
                for (auto &source : list->sources) {
                    auto e = source.first.Get(lock);
                    double value = ecs::SignalBindings::GetSignal(lock, e, source.second);
                    if (e) {
                        Logf("      %s on %s: %.2f", source.second, ecs::ToString(lock, e), value);
                    } else {
                        Logf("      %s on %s(missing): %.2f", source.second, source.first.Name(), value);
                    }
                }
            }
        }
    }
} // namespace sp
