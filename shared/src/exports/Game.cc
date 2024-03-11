/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "game/Game.hh"

#include "ecs/EcsImpl.hh"
#include "game/CGameContext.hh"

#include <cxxopts.hpp>
#include <filesystem>
#include <strayphotons.h>

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

SP_EXPORT int sp_game_start(sp_game_t game) {
    Assertf(game != nullptr, "sp_game_start called with null game");
    try {
        return game->game.Start();
    } catch (const std::exception &e) {
        Abortf("Error invoking game.Start(): %s", e.what());
    }
}

SP_EXPORT void sp_game_trigger_exit(sp_game_t game) {
    Assertf(game != nullptr, "sp_game_trigger_exit called with null game");
    game->game.exitTriggered.test_and_set();
    game->game.exitTriggered.notify_all();
}

SP_EXPORT bool sp_game_is_exit_triggered(sp_game_t game) {
    Assertf(game != nullptr, "sp_game_is_exit_triggered called with null game");
    return game->game.exitTriggered.test();
}

SP_EXPORT int sp_game_wait_for_exit_trigger(sp_game_t game) {
    Assertf(game != nullptr, "sp_game_wait_for_exit_trigger called with null game");
    while (!game->game.exitTriggered.test()) {
        game->game.exitTriggered.wait(false);
    }
    return game->game.exitCode;
}

SP_EXPORT int sp_game_get_exit_code(sp_game_t game) {
    Assertf(game != nullptr, "sp_game_get_exit_code called with null game");
    return game->game.exitCode;
}

SP_EXPORT void sp_game_destroy(sp_game_t game) {
    Assertf(game != nullptr, "sp_game_destroy called with null game");
    delete game;
}

SP_EXPORT cxxopts::ParseResult *sp_game_get_options(sp_game_t game) {
    Assertf(game != nullptr, "sp_game_get_options called with null game");
    return &game->game.options;
}

SP_EXPORT void sp_game_set_graphics_manager(sp_game_t game,
    GraphicsManager *manager,
    void (*destroy_callback)(GraphicsManager *)) {
    Assertf(game != nullptr, "sp_game_set_shutdown_callback called with null game");
    if (manager) {
        game->game.graphics = std::shared_ptr<GraphicsManager>(manager, destroy_callback);
    } else {
        game->game.graphics.reset();
    }
}

SP_EXPORT GraphicsManager *sp_game_get_graphics_manager(sp_game_t game) {
    Assertf(game != nullptr, "sp_game_get_graphics_manager called with null game");
    return game->game.graphics.get();
}

SP_EXPORT void sp_game_set_input_handler(sp_game_t game, void *handler, void (*destroy_callback)(void *)) {
    Assertf(game != nullptr, "sp_game_set_input_handler called with null game");
    if (handler) {
        game->inputHandler = std::shared_ptr<void>(handler, destroy_callback);
    } else {
        game->inputHandler.reset();
    }
}
SP_EXPORT void *sp_game_get_input_handler(sp_game_t game) {
    Assertf(game != nullptr, "sp_game_get_input_handler called with null game");
    return game->inputHandler.get();
}
