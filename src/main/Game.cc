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
#include <filesystem>
#include <glm/glm.hpp>
#include <strayphotons.h>
#include <wasm.rs.h>

namespace sp {
    std::atomic_int GameExitCode;
    std::atomic_flag GameExitTriggered;

    CFunc<int> cfExit("exit", "Quits the game", [](int arg) {
        Tracef("Exit triggered via console command");
        GameExitCode = arg;
        GameExitTriggered.test_and_set();
        GameExitTriggered.notify_all();
    });

    Game::Game(cxxopts::ParseResult &options)
        : options(options),
#ifdef SP_GRAPHICS_SUPPORT
          graphics(*this),
#endif
#ifdef SP_PHYSICS_SUPPORT_PHYSX
          physics(windowEventQueue),
#endif
#ifdef SP_XR_SUPPORT
          xr(this),
#endif
#ifdef SP_AUDIO_SUPPORT
          audio(new AudioManager),
#endif
          logic(windowEventQueue) {
    }

    const int64 MaxInputPollRate = 144;

    Game::~Game() {}

    Game::ShutdownManagers::~ShutdownManagers() {
        GetConsoleManager().Shutdown();
        GetSceneManager().Shutdown();
        Assets().Shutdown();
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

        bool scriptMode = false;
        if (options.count("run")) {
            scriptMode = true;
            logic.DisableInput();
        }

#ifdef SP_GRAPHICS_SUPPORT
        if (!options.count("headless")) {
            graphics.Init();

            debugGui = std::make_unique<DebugGuiManager>();
            menuGui = std::make_unique<MenuGuiManager>(this->graphics);

            graphics.StartThread(scriptMode);
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

        if (scriptMode) {
            string scriptPath = options["run"].as<string>();

            Logf("Executing commands from file: %s", scriptPath);
            auto asset = sp::Assets().Load("scripts/" + scriptPath)->Get();
            if (!asset) {
                Errorf("Command file not found: %s", scriptPath);
                return 1;
            }

            sp::ConsoleScript startupScript(scriptPath, asset);
            funcs.Register<int>("sleep", "Pause command execution for N milliseconds", [](int ms) {
                std::this_thread::sleep_for(std::chrono::milliseconds(ms));
            });
            funcs.Register<int>("syncscene", "Pause command execution until all scenes are loaded", [](int count) {
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

            Debugf("Running console script: %s", startupScript.path);
            GetConsoleManager().StartThread(&startupScript);
        } else {
            if (options.count("scene")) {
                scenes.QueueAction(SceneAction::LoadScene, options["scene"].as<string>());
            } else {
                scenes.QueueAction(SceneAction::LoadScene, "menu");
            }
            GetConsoleManager().StartThread();
        }

        logic.StartThread(scriptMode);
        physics.StartThread(scriptMode);

#ifdef SP_GRAPHICS_SUPPORT
        if (!options.count("headless")) {
    #ifdef SP_INPUT_SUPPORT_WINIT
            auto *inputHandler = graphics.GetWinitInputHandler();
            Assertf(inputHandler != nullptr, "WinitInputHandler is null");
            inputHandler->StartEventLoop((uint32_t)MaxInputPollRate);
            graphics.StopThread();
    #else
            auto frameEnd = chrono_clock::now();
            while (!GameExitTriggered.test()) {
                static const char *frameName = "WindowInput";
                (void)frameName;
                FrameMarkStart(frameName);
                if (scriptMode) {
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
                FrameMarkEnd(frameName);
            }
            graphics.StopThread();
    #endif
        } else {
            while (!GameExitTriggered.test()) {
                GameExitTriggered.wait(false);
            }
        }
#else
        while (!GameExitTriggered.test()) {
            GameExitTriggered.wait(false);
        }
#endif
        return GameExitCode;
    }

    struct CGameContext {
        cxxopts::ParseResult options;
        Game game;

#ifdef _WIN32
        std::shared_ptr<unsigned int> winSchedulerHandle;
#endif

#ifdef _WIN32
        CGameContext(cxxopts::ParseResult &&options)
            : options(std::move(options)), game(this->options), winSchedulerHandle(SetWindowsSchedulerFix()) {}
#else
        CGameContext(cxxopts::ParseResult &&options) : options(std::move(options)), game(this->options) {}
#endif
    };

    StrayPhotons game_init(int argc, char **argv) {
        using cxxopts::value;

#ifdef CATCH_GLOBAL_EXCEPTIONS
        try
#endif
        {
            cxxopts::Options options("strayphotons", "Stray Photons Game Engine\n");
            options.allow_unrecognised_options();

            // clang-format off
            options.add_options()
                ("h,help", "Display help")
                ("r,run", "Load commands from a file an execute them in the console", value<string>())
                ("s,scene", "Initial scene to load", value<string>())
                ("size", "Initial window size", value<string>())
#ifdef SP_XR_SUPPORT
                ("no-vr", "Disable automatic XR/VR system loading")
#endif
#ifdef SP_GRAPHICS_SUPPORT
                ("headless", "Disable window creation and graphics initialization")
                ("with-validation-layers", "Enable Vulkan validation layers")
#endif
                ("c,command", "Run a console command on init", value<vector<string>>());
            // clang-format on

            auto optionsResult = options.parse(argc, argv);

            if (optionsResult.count("help")) {
                std::cout << options.help() << std::endl;
                return nullptr;
            }

            Logf("Starting in directory: %s", std::filesystem::current_path().string());
            return new CGameContext(std::move(optionsResult));
        }
#ifdef CATCH_GLOBAL_EXCEPTIONS
        catch (const char *err) {
            Errorf("terminating with exception: %s", err);
        } catch (const std::exception &ex) {
            Errorf("terminating with exception: %s", ex.what());
        }
#endif
        return nullptr;
    }

    int game_start(StrayPhotons instance) {
        Assertf(instance != nullptr, "sp::game_destroy called with null instance");

        try {
            return instance->game.Start();
        } catch (const std::exception &e) {
            Abortf("Error invoking game.Start(): %s", e.what());
        }
    }

    void game_destroy(StrayPhotons instance) {
        Assertf(instance != nullptr, "sp::game_destroy called with null instance");

        delete instance;
    }
} // namespace sp

#ifdef _WIN32
    #include <windows.h>

std::shared_ptr<unsigned int> SetWindowsSchedulerFix() {
    // Increase thread scheduler resolution from default of 15ms
    timeBeginPeriod(1);
    return std::shared_ptr<UINT>(new UINT(1), [](UINT *period) {
        timeEndPeriod(*period);
        delete period;
    });
}
#endif
