/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Game.hh"

#include "assets/AssetManager.hh"
#include "assets/ConsoleScript.hh"
#include "console/Console.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "core/RegisteredThread.hh"
#include "core/Tracing.hh"
#include "ecs/Ecs.hh"
#include "ecs/EcsImpl.hh"
#include "game/SceneManager.hh"

#ifdef SP_GRAPHICS_SUPPORT
    #include "graphics/core/GraphicsContext.hh"
#endif

#ifdef SP_AUDIO_SUPPORT
    #include "audio/AudioManager.hh"
#endif

#include <atomic>
#include <cxxopts.hpp>
#include <glm/glm.hpp>
#include <strayphotons.h>
#include <wasm.rs.h>

namespace sp {
    std::atomic_int gameExitCode;
    std::atomic_flag gameExitTriggered;

    CFunc<int> cfExit("exit", "Quits the game", [](int arg) {
        Tracef("Exit triggered via console command");
        gameExitCode = arg;
        gameExitTriggered.test_and_set();
        gameExitTriggered.notify_all();
    });

    Game::Game(cxxopts::ParseResult &options, const ConsoleScript *startupScript)
        : options(options), startupScript(startupScript),
#ifdef SP_GRAPHICS_SUPPORT
          graphics(this, startupScript != nullptr),
#endif
#ifdef SP_PHYSICS_SUPPORT_PHYSX
          physics(windowEventQueue, startupScript != nullptr),
#endif
#ifdef SP_XR_SUPPORT
          xr(this),
#endif
#ifdef SP_AUDIO_SUPPORT
          audio(new AudioManager),
#endif
          logic(windowEventQueue, startupScript != nullptr) {
    }

    const int64 MaxInputPollRate = 144;

    Game::~Game() {}

    Game::ShutdownManagers::~ShutdownManagers() {
        GetConsoleManager().Shutdown();
        GetSceneManager().Shutdown();
    }

    int Game::Start() {
        tracy::SetThreadName("Main");

        Debugf("Bytes of memory used per entity: %u", ecs::World().GetBytesPerEntity());

        {
            auto lock = ecs::StartTransaction<ecs::AddRemove>();
            lock.Set<ecs::FocusLock>();
            lock.Set<ecs::ActiveScene>();
            lock.Set<ecs::Signals>();
        }
        {
            auto lock = ecs::StartStagingTransaction<ecs::AddRemove>();
            lock.Set<ecs::Signals>();
        }

        GetConsoleManager().StartInputLoop();

        sp::wasm::print_hello();

        if (options.count("command")) {
            for (auto &cmdline : options["command"].as<vector<string>>()) {
                GetConsoleManager().ParseAndExecute(cmdline);
            }
        }

#ifdef SP_GRAPHICS_SUPPORT
        if (!options.count("headless")) {
            graphics.Init();

            debugGui = std::make_unique<DebugGuiManager>();
            menuGui = std::make_unique<MenuGuiManager>(this->graphics);

            graphics.StartThread();
        }
#endif

#ifdef SP_XR_SUPPORT
        if (options.count("no-vr") == 0) xr.LoadXrSystem();
#endif

        auto &scenes = GetSceneManager();
#ifdef SP_GRAPHICS_SUPPORT
        if (options.count("headless")) {
#endif
            scenes.DisableGraphicsPreload();
#ifdef SP_GRAPHICS_SUPPORT
        }
#endif
#ifndef SP_PHYSICS_SUPPORT_PHYSX
        scenes.DisablePhysicsPreload();
#endif
        scenes.QueueAction(SceneAction::ReloadPlayer);
        scenes.QueueAction(SceneAction::ReloadBindings);

        if (startupScript != nullptr) {
            funcs.Register<int>("sleep", "Pause script execution for N milliseconds", [](int ms) {
                std::this_thread::sleep_for(std::chrono::milliseconds(ms));
            });
            funcs.Register<int>("syncscene", "Pause script until all scenes are loaded", [](int count) {
                if (count < 1) count = 1;
                for (int i = 0; i < count; i++) {
                    GetSceneManager().QueueActionAndBlock(SceneAction::SyncScene);
                }
            });
            funcs.Register<unsigned int>("stepgraphics",
                "Renders N frames in a row, saving any queued screenshots, default is 1",
                [this](unsigned int arg) {
                    auto count = std::max(1u, arg);
                    for (auto i = 0u; i < count; i++) {
                        // Step main thread glfw input first
                        graphicsMaxStepCount++;
                        auto step = graphicsStepCount.load();
                        while (step < graphicsMaxStepCount) {
                            graphicsStepCount.wait(step);
                            step = graphicsStepCount.load();
                        }

                        graphics.Step(1);
                    }
                });

            GetConsoleManager().QueueParseAndExecute("syncscene");

            Debugf("Running script: %s", startupScript->path);
        } else {
            if (options.count("map")) {
                scenes.QueueAction(SceneAction::LoadScene, options["map"].as<string>());
            } else {
                scenes.QueueAction(SceneAction::LoadScene, "menu");
            }
        }

        GetConsoleManager().StartThread(startupScript);
        logic.StartThread();

#ifdef SP_GRAPHICS_SUPPORT
        auto frameEnd = chrono_clock::now();
        while (!gameExitTriggered.test()) {
            // static const char *frameName = "WindowInput";
            // FrameMarkStart(frameName);
            if (startupScript) {
                while (graphicsStepCount < graphicsMaxStepCount) {
                    graphics.InputFrame();
                    graphicsStepCount++;
                }
                graphicsStepCount.notify_all();
            } else if (!graphics.InputFrame()) {
                Tracef("Exit triggered via window manager");
                break;
            }

            auto realFrameEnd = chrono_clock::now();
            auto interval = graphics.interval;
            if (interval.count() == 0) {
                interval = std::chrono::nanoseconds((int64)(1e9 / MaxInputPollRate));
            }

            frameEnd += interval;

            if (realFrameEnd >= frameEnd) {
                // Falling behind, reset target frame end time.
                // Add some extra time to allow other threads to start transactions.
                frameEnd = realFrameEnd + std::chrono::nanoseconds(100);
            }

            std::this_thread::sleep_until(frameEnd);
            // FrameMarkEnd(frameName);
        }
        graphics.StopThread();
#else
        while (!exitTriggered.test()) {
            exitTriggered.wait(false);
        }
#endif
        return gameExitCode;
    }

    struct CGameContext {
        cxxopts::ParseResult optionsResult;
        Game game;

        CGameContext(cxxopts::Options options, int argc, char **argv)
            : optionsResult(options.parse(argc, argv)), game(optionsResult) {}
    };

    StrayPhotons game_init(int argc, char **argv) {
        cxxopts::Options options("sp-vk", "Stray Photons Game Engine\n");
        CGameContext *instance = new CGameContext(options, argc, argv);

        static_assert(sizeof(uintptr_t) <= sizeof(StrayPhotons), "Pointer size larger than handle");
        return static_cast<StrayPhotons>(reinterpret_cast<uintptr_t>(instance));
    }

    int game_start(StrayPhotons ctx) {
        CGameContext *instance = reinterpret_cast<CGameContext *>(static_cast<uintptr_t>(ctx));
        Assertf(instance != nullptr, "sp::game_destroy called with null instance");

        try {
            return instance->game.Start();
        } catch (const std::exception &e) {
            Abortf("Error invoking game.Start(): %s", e.what());
        }
    }

    void game_destroy(StrayPhotons ctx) {
        CGameContext *instance = reinterpret_cast<CGameContext *>(static_cast<uintptr_t>(ctx));
        Assertf(instance != nullptr, "sp::game_destroy called with null instance");

        delete instance;
    }
} // namespace sp
