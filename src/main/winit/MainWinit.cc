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
// #include "game/CGameContext.hh"
// #include "game/SceneManager.hh"

#include <strayphotons.h>

struct OnStart {
    OnStart() {
        // ecs::GetECSContext(sp::game_get_ecs_context());
        // ecs::GetScriptManager(sp::game_get_script_manager());
        // ecs::GetScriptDefinitions(sp::game_get_script_definitons());
        // sp::Assets(sp::game_get_asset_manager());
        // sp::GetSceneManager(sp::game_get_scene_manager());
    }
} onStart;

#include "WinitInputHandler.hh"
#include "common/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "game/Game.hh"
#include "graphics/GraphicsManager.hh"
#include "graphics/core/GraphicsContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"

#ifdef SP_XR_SUPPORT
    #include "xr/XrManager.hh"
#endif

#include <csignal>
#include <cstdio>
#include <cxxopts.hpp>
#include <filesystem>
#include <memory>

using cxxopts::value;

namespace sp {
    Game *GameInstance = nullptr;

    void handleSignals(int signal) {
        if (signal == SIGINT && GameInstance != nullptr) {
            GameInstance->exitTriggered.test_and_set();
            GameInstance->exitTriggered.notify_all();
        }
    }

    void destroyGraphicsCallback(sp_game_t ctx) {
        if (ctx != nullptr) {
#ifdef SP_XR_SUPPORT
            ctx->game.xr.reset();
#endif

            if (ctx->inputHandler) {
                WinitInputHandler *handler = (WinitInputHandler *)ctx->inputHandler;
                delete handler;
                ctx->inputHandler = nullptr;
            }
            ctx->game.graphics.reset();
        }
    }
} // namespace sp

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

    std::shared_ptr<std::remove_pointer_t<sp_game_t>> instance(sp_game_init(ARGC_NAME, ARGV_NAME), [](sp_game_t ctx) {
        sp::GameInstance = nullptr;
        sp_game_destroy(ctx);
    });

    sp::CGameContext *ctx = instance.get();
    sp::GameInstance = &ctx->game;

    {
        using namespace sp;
        auto &game = ctx->game;

        bool withValidationLayers = game.options.count("with-validation-layers");
        game.graphics.context = make_shared<vulkan::DeviceContext>(game, withValidationLayers);
        ctx->inputHandler = new WinitInputHandler(ctx);

#ifdef SP_XR_SUPPORT
        if (!game.options.count("no-vr")) {
            game.xr = make_shared<xr::XrManager>(&game);
            game.xr->LoadXrSystem();
        }
#endif

        sp_game_set_shutdown_callback(ctx, &sp::destroyGraphicsCallback);

        int status_code = sp_game_start(ctx);
        if (status_code) return status_code;

        if (!game.options.count("headless")) {
            Assertf(ctx->inputHandler != nullptr, "WinitInputHandler is null");
            auto *inputHandler = (WinitInputHandler *)ctx->inputHandler;
            inputHandler->StartEventLoop((uint32_t)MaxInputPollRate);
        } else {
            while (!game.exitTriggered.test()) {
                game.exitTriggered.wait(false);
            }
        }
        return game.exitCode;
    }
}
