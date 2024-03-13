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

    void PrepareWindowView(GraphicsManager *graphics, int *width_out, int *height_out) {
        bool fullscreen = CVarWindowFullscreen.Get();
        if (systemFullscreen != fullscreen) {
            if (fullscreen) {
                sp::winit::get_window_position(*winitContext, &storedWindowRect.x, &storedWindowRect.y);
                storedWindowRect.z = systemWindowSize.x;
                storedWindowRect.w = systemWindowSize.y;

                auto monitor = sp::winit::get_active_monitor(*winitContext);
                glm::uvec2 readWindowSize;
                sp::winit::get_monitor_resolution(*monitor, &readWindowSize.x, &readWindowSize.y);
                if (readWindowSize != glm::uvec2(0)) systemWindowSize = readWindowSize;
                sp::winit::set_window_mode(*winitContext,
                    &*monitor,
                    0,
                    0,
                    (uint32_t)systemWindowSize.x,
                    (uint32_t)systemWindowSize.y);
            } else {
                systemWindowSize = {storedWindowRect.z, storedWindowRect.w};
                sp::winit::set_window_mode(*winitContext,
                    0,
                    storedWindowRect.x,
                    storedWindowRect.y,
                    (uint32_t)storedWindowRect.z,
                    (uint32_t)storedWindowRect.w);
            }
            CVarWindowSize.Set(systemWindowSize);
            systemFullscreen = fullscreen;
        }

        glm::ivec2 windowSize = CVarWindowSize.Get();
        if (systemWindowSize != windowSize) {
            if (CVarWindowFullscreen.Get()) {
                auto monitor = sp::winit::get_active_monitor(*winitContext);
                sp::winit::set_window_mode(*winitContext, &*monitor, 0, 0, windowSize.x, windowSize.y);
            } else {
                sp::winit::set_window_inner_size(*winitContext, windowSize.x, windowSize.y);
            }

            systemWindowSize = windowSize;
        }

        glm::uvec2 fbExtents;
        sp::winit::get_window_inner_size(*winitContext, &fbExtents.x, &fbExtents.y);
        if (fbExtents.x > 0 && fbExtents.y > 0) {
            *width_out = fbExtents.x;
            *height_out = fbExtents.y;
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
