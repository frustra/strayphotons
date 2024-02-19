/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "game/Game.hh"

#include "assets/AssetManager.hh"
#include "console/Console.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/ScriptManager.hh"
#include "ecs/SignalManager.hh"
#include "game/CGameContext.hh"
#include "game/SceneManager.hh"

#include <cxxopts.hpp>
#include <filesystem>
#include <strayphotons.h>
#include <strayphotons/export.h>

SP_EXPORT sp_game_t sp_game_init(int argc, char **argv) {
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
            ("no-vr", "Disable automatic XR/VR system loading")
            ("headless", "Disable window creation and graphics initialization")
            ("with-validation-layers", "Enable Vulkan validation layers")
            ("c,command", "Run a console command on init", value<vector<string>>());
        // clang-format on

        auto optionsResult = options.parse(argc, argv);

        if (optionsResult.count("help")) {
            std::cout << options.help() << std::endl;
            return nullptr;
        }

        Logf("Starting in directory: %s", std::filesystem::current_path().string());
        // When running a script, disable input events from the window
        return new CGameContext(std::move(optionsResult), optionsResult.count("run"));
    }
#ifdef CATCH_GLOBAL_EXCEPTIONS
    catch (const char *err) {
        Errorf("terminating with exception: %s", err);
    } catch (const std::exception &ex) {
        Errorf("terminating with exception: %s", ex.what());
    }
    return nullptr;
#endif
}

SP_EXPORT void sp_game_set_shutdown_callback(sp_game_t ctx, void (*callback)(sp_game_t)) {
    Assertf(ctx != nullptr, "sp_game_set_shutdown_callback called with null ctx");
    ctx->game.shutdownCallback = callback;
}

SP_EXPORT GraphicsContext *sp_game_get_graphics_context(sp_game_t ctx) {
    Assertf(ctx != nullptr, "sp_get_graphics_manager called with null ctx");
    return ctx->game.graphics.context.get();
}

SP_EXPORT cxxopts::ParseResult *sp_game_get_options(sp_game_t ctx) {
    Assertf(ctx != nullptr, "sp_game_get_options called with null ctx");
    return &ctx->game.options;
}

SP_EXPORT int sp_game_start(sp_game_t ctx) {
    Assertf(ctx != nullptr, "sp_game_start called with null ctx");
    try {
        return ctx->game.Start();
    } catch (const std::exception &e) {
        Abortf("Error invoking game.Start(): %s", e.what());
    }
}

SP_EXPORT void sp_game_destroy(sp_game_t ctx) {
    Assertf(ctx != nullptr, "sp_game_destroy called with null ctx");
    delete ctx;
}

SP_EXPORT ConsoleManager *sp_get_console_manager() {
    return GetConsoleManager();
}

SP_EXPORT ecs::ECSContext *sp_get_ecs_context() {
    return ecs::GetECSContext();
}

SP_EXPORT ecs::SignalManager *sp_get_signal_manager() {
    return ecs::GetSignalManager();
}

SP_EXPORT ecs::ScriptManager *sp_get_script_manager() {
    return ecs::GetScriptManager();
}

SP_EXPORT ecs::ScriptDefinitions *sp_get_script_definitons() {
    return ecs::GetScriptDefinitions();
}

SP_EXPORT AssetManager *sp_get_asset_manager() {
    return Assets();
}

SP_EXPORT SceneManager *sp_get_scene_manager() {
    return GetSceneManager();
}
