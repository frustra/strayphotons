/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "game/Game.hh"

#include "assets/AssetManager.hh"
#include "assets/ConsoleScript.hh"
#include "common/Common.hh"
#include "common/Logging.hh"
#include "common/RegisteredThread.hh"
#include "common/Tracing.hh"
#include "console/Console.hh"
#include "ecs/Ecs.hh"
#include "ecs/EcsImpl.hh"
#include "game/CGameContext.hh"
#include "game/SceneManager.hh"

#include <atomic>
#include <cxxopts.hpp>
#include <glm/glm.hpp>
#include <strayphotons.h>

#ifdef SP_RUST_WASM_SUPPORT
    #include <wasm.rs.h>
#endif

namespace sp {

    Game::Game(CGameContext &ctx) : gameContext(ctx), options(ctx.options), logic(inputEventQueue) {
        funcs.Register<int>("exit", "Quits the game", [this](int arg) {
            Tracef("Exit triggered via console command");
            this->exitCode = arg;
            this->exitTriggered.test_and_set();
            this->exitTriggered.notify_all();
        });

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

        if (options.count("command")) {
            for (auto &cmdline : options["command"].as<vector<string>>()) {
                GetConsoleManager().ParseAndExecute(cmdline);
            }
        }

        InitGraphicsManager(*this);
        InitPhysicsManager(*this);
        InitAudioManager(*this);
    }

    Game::ShutdownManagers::~ShutdownManagers() {
        GetConsoleManager().Shutdown();
        GetSceneManager().Shutdown();
        Assets().Shutdown();
    }

    int Game::Start() {
        tracy::SetThreadName("Main");

        GetConsoleManager().StartInputLoop();

#ifdef SP_RUST_WASM_SUPPORT
        sp::wasm::print_hello();
#endif

        bool scriptMode = options.count("run") > 0;
        StartGraphicsThread(*this, scriptMode);
        StartPhysicsThread(*this, scriptMode);

        auto &scenes = GetSceneManager();
        if (!graphics) scenes.DisableGraphicsPreload();
        if (!physics) scenes.DisablePhysicsPreload();
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

        return exitCode;
    }
} // namespace sp
