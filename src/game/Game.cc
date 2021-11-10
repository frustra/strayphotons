#include "Game.hh"

#include "assets/Script.hh"
#include "console/Console.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "core/RegisteredThread.hh"
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

        {
            auto lock = ecs::World.StartTransaction<ecs::AddRemove>();
            lock.Set<ecs::FocusLock>();
        }

#if RUST_CXX
        sp::rust::print_hello();
#endif

        try {
#ifdef SP_GRAPHICS_SUPPORT
            debugGui = std::make_unique<DebugGuiManager>();
            menuGui = std::make_unique<MenuGuiManager>(this->graphics);

            graphics.Init();
#endif

#ifdef SP_XR_SUPPORT
            if (options["no-vr"].count() == 0) xr.LoadXrSystem();
#endif

            auto &scenes = GetSceneManager();
            ReloadPlayer();
            if (options.count("map")) { scenes.LoadScene(options["map"].as<string>()); }

            if (startupScript != nullptr) {
                startupScript->Exec();
            } else if (!options.count("map")) {
                scenes.LoadScene("menu");
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
        scenes.LoadPlayer();

#ifdef SP_GRAPHICS_SUPPORT
        graphics.GetContext()->AttachView(scenes.GetPlayer());
#endif

        scenes.RespawnPlayer();
        scenes.LoadBindings();
    }

    void Game::PrintDebug() {
        auto lock = ecs::World.StartTransaction<ecs::Read<ecs::Name,
            ecs::Transform,
            ecs::HumanController,
            ecs::LightSensor,
            ecs::SignalOutput,
            ecs::EventInput,
            ecs::FocusLayer,
            ecs::FocusLock>>();
        auto &scenes = GetSceneManager();
        auto player = scenes.GetPlayer();
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
} // namespace sp
