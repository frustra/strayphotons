/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifdef _WIN32
    #include <windows.h>
#endif

#include <iostream>
using namespace std;

// #include "assets/AssetManager.hh"
// #include "console/Console.hh"
// #include "ecs/Ecs.hh"
// #include "ecs/ScriptManager.hh"
// #include "ecs/SignalManager.hh"
// #include "game/Game.hh"
// #include "game/SceneManager.hh"

#include <strayphotons.h>

struct OnStart {
    OnStart() {
        // ecs::GetECSContext(sp_get_ecs_context());
        // ecs::GetSignalManager(sp_get_signal_manager());
        // ecs::GetScriptManager(sp_get_script_manager());
        // ecs::GetScriptDefinitions(sp_get_script_definitons());
        // sp::Assets(sp_get_asset_manager());
        // sp::GetSceneManager(sp_get_scene_manager());
    }
} onStart;

#include "GlfwInputHandler.hh"
#include "common/Common.hh"
#include "common/Logging.hh"
// #include "ecs/EcsImpl.hh"
// #include "game/Game.hh"
// #include "graphics/core/GraphicsManager.hh"
// #include "graphics/vulkan/core/DeviceContext.hh"

#ifdef SP_PHYSICS_SUPPORT_PHYSX
    #include "physx/PhysxManager.hh"
#endif

#ifdef SP_AUDIO_SUPPORT
    #include "audio/AudioManager.hh"
#endif

#ifdef SP_XR_SUPPORT
    #include "xr/XrManager.hh"
#endif

#include <csignal>
#include <cstdio>
#include <cxxopts.hpp>
#include <filesystem>
#include <memory>
#include <strayphotons.h>

using cxxopts::value;

namespace sp {
    sp_game_t GameInstance = (sp_game_t)0;
    std::shared_ptr<GraphicsManager> GameGraphicsManager;
    std::shared_ptr<GlfwInputHandler> GameInputHandler;

    void handleSignals(int signal) {
        if (signal == SIGINT && GameInstance) {
            sp_game_trigger_exit(GameInstance);
        }
    }

    // void destroyGraphicsCallback(sp_game_t ctx) {
    //     if (ctx != nullptr) {
    //         ctx->game.xr.reset();
    //         if (ctx->inputHandler) {
    //             GlfwInputHandler *handler = (GlfwInputHandler *)ctx->inputHandler;
    //             delete handler;
    //             ctx->inputHandler = nullptr;
    //         }
    //     }
    // }
} // namespace sp

using namespace sp;

const int64_t MaxInputPollRate = 144;

// TODO: Commented until package release saves a log file
// #if defined(_WIN32) && defined(SP_PACKAGE_RELEASE)
//     #define ARGC_NAME __argc
//     #define ARGV_NAME __argv
// int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
// #else
#define ARGC_NAME argc
#define ARGV_NAME argv
int main(int argc, char **argv)
// #endif
{

#ifdef _WIN32
    signal(SIGINT, sp::handleSignals);
#else
    struct sigaction act;
    act.sa_handler = sp::handleSignals;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGINT, &act, 0);
#endif

    std::shared_ptr<std::remove_pointer_t<sp_game_t>> instance(sp_game_init(ARGC_NAME, ARGV_NAME), [](sp_game_t game) {
        GameInstance = nullptr;
        sp_game_destroy(game);
    });

    GameInstance = instance.get();
    Logf("Game instance: %x", GameInstance);
    // auto *optionsPtr = sp_game_get_options(GameInstance);
    // Assertf(optionsPtr, "Game instance has no parsed options");
    // cxxopts::ParseResult &options = *optionsPtr;

    // if (!options.count("headless")) {
    //     GameGraphicsManager = std::make_shared<GraphicsManager>(GameInstance);
    //     sp_game_set_graphics_manager(GameInstance, GameGraphicsManager.get(), [](GraphicsManager *manager) {
    //         Assertf(manager == GameGraphicsManager.get(), "GraphicsManager pointer mismatch");
    //         GameGraphicsManager.reset();
    //     });
    // }
    // sp_game_set_shutdown_callback(GameInstance, &sp::destroyGraphicsCallback);

    // #ifdef SP_PHYSICS_SUPPORT_PHYSX
    //     game.physics = make_shared<PhysxManager>(game.inputEventQueue);
    // #endif
    // #ifdef SP_AUDIO_SUPPORT
    //     game.audio = make_shared<AudioManager>();
    // #endif

    // if (GameGraphicsManager) GameGraphicsManager->Init();

    // bool withValidationLayers = options.count("with-validation-layers");
    // auto deviceContext = std::make_shared<vulkan::DeviceContext>(withValidationLayers);
    // GameGraphicsManager->context = deviceContext;

    // GameInputHandler = std::make_shared<GlfwInputHandler>(GameInstance, deviceContext->GetGlfwWindow());
    // sp_game_set_input_handler(GameInstance, GameInputHandler.get(), [](void *handler) {
    //     Assertf(handler == GameInputHandler.get(), "InputHandler pointer mismatch");
    //     GameInputHandler.reset();
    // });

    // #ifdef SP_XR_SUPPORT
    //     if (!options.count("no-vr")) {
    //         game.xr = make_shared<xr::XrManager>(&game);
    //         game.xr->LoadXrSystem();
    //     }
    // #endif

    // bool scriptMode = options.count("run") > 0;
    // if (GameGraphicsManager) GameGraphicsManager->StartThread(scriptMode);

    int status_code = sp_game_start(GameInstance);
    if (status_code) return status_code;

    // #ifdef SP_PHYSICS_SUPPORT_PHYSX
    //     game.physics->StartThread(scriptMode);
    // #endif

    if (GameGraphicsManager) {
        // std::atomic_uint64_t graphicsStepCount, graphicsMaxStepCount;

        // if (scriptMode) {
        //     game.funcs.Register<unsigned int>("stepgraphics",
        //         "Renders N frames in a row, saving any queued screenshots, default is 1",
        //         [&game](unsigned int arg) {
        //             auto count = std::max(1u, arg);
        //             for (auto i = 0u; i < count; i++) {
        //                 // Step main thread glfw input first
        //                 graphicsMaxStepCount++;
        //                 auto step = graphicsStepCount.load();
        //                 while (step < graphicsMaxStepCount) {
        //                     graphicsStepCount.wait(step);
        //                     step = graphicsStepCount.load();
        //                 }

        //                 GameGraphicsManager->Step(1);
        //             }
        //         });
        // }

        // auto frameEnd = chrono_clock::now();
        // while (!sp_game_is_exit_triggered(GameInstance)) {
        //     static const char *frameName = "WindowInput";
        //     (void)frameName;
        //     FrameMarkStart(frameName);
        //     if (scriptMode) {
        //         while (graphicsStepCount < graphicsMaxStepCount) {
        //             GlfwInputHandler::Frame();
        //             GameGraphicsManager->InputFrame();
        //             graphicsStepCount++;
        //         }
        //         graphicsStepCount.notify_all();
        //     } else {
        //         GlfwInputHandler::Frame();
        //         if (!GameGraphicsManager->InputFrame()) {
        //             Tracef("Exit triggered via window manager");
        //             break;
        //         }
        //     }

        //     auto realFrameEnd = chrono_clock::now();
        //     auto interval = GameGraphicsManager->interval;
        //     if (interval.count() == 0) {
        //         interval = std::chrono::nanoseconds((int64_t)(1e9 / MaxInputPollRate));
        //     }

        //     frameEnd += interval;

        //     if (realFrameEnd >= frameEnd) {
        //         // Falling behind, reset target frame end time.
        //         // Add some extra time to allow other threads to start transactions.
        //         frameEnd = realFrameEnd + std::chrono::nanoseconds(100);
        //     }

        //     std::this_thread::sleep_until(frameEnd);
        //     FrameMarkEnd(frameName);
        // }
        return sp_game_get_exit_code(GameInstance);
    } else {
        return sp_game_wait_for_exit_trigger(GameInstance);
    }
}
