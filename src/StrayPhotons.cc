/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "assets/AssetManager.hh"
#include "console/Console.hh"
#include "ecs/Ecs.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/ScriptManager.hh"
#include "game/CGameContext.hh"
#include "game/Game.hh"
#include "game/SceneManager.hh"

#include <cxxopts.hpp>
#include <filesystem>
#include <sp_export.h>
#include <strayphotons.h>

namespace sp {
    SP_EXPORT StrayPhotons game_init(int argc, char **argv) {
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

    SP_EXPORT void game_set_shutdown_callback(StrayPhotons ctx, void (*callback)(StrayPhotons)) {
        Assertf(ctx != nullptr, "sp::game_set_shutdown_callback called with null ctx");
        ctx->game.shutdownCallback = callback;
    }

    SP_EXPORT int game_start(StrayPhotons ctx) {
        Assertf(ctx != nullptr, "sp::game_start called with null ctx");
        try {
            return ctx->game.Start();
        } catch (const std::exception &e) {
            Abortf("Error invoking game.Start(): %s", e.what());
        }
    }

    SP_EXPORT void game_destroy(StrayPhotons ctx) {
        Assertf(ctx != nullptr, "sp::game_destroy called with null ctx");
        delete ctx;
    }

    SP_EXPORT ConsoleManager *game_get_console_manager() {
        return GetConsoleManager();
    }

    SP_EXPORT ecs::ECSContext *game_get_ecs_context() {
        return ecs::GetECSContext();
    }

    SP_EXPORT ecs::ScriptManager *game_get_script_manager() {
        return ecs::GetScriptManager();
    }

    SP_EXPORT ecs::ScriptDefinitions *game_get_script_definitons() {
        return ecs::GetScriptDefinitions();
    }

    SP_EXPORT AssetManager *game_get_asset_manager() {
        return Assets();
    }

    SP_EXPORT SceneManager *game_get_scene_manager() {
        return GetSceneManager();
    }

    SP_EXPORT uint64_t game_new_input_device(StrayPhotons ctx, const char *name) {
        Assertf(ctx != nullptr, "sp::game_new_input_device called with null ctx");
        ecs::EntityRef inputEntity = ecs::Name("input", name);
        GetSceneManager()->QueueActionAndBlock(SceneAction::ApplySystemScene,
            "input",
            [&](ecs::Lock<ecs::AddRemove> lock, std::shared_ptr<Scene> scene) {
                auto keyboard = scene->NewSystemEntity(lock, scene, inputEntity.Name());
                keyboard.Set<ecs::EventBindings>(lock);
            });
        return (uint64_t)inputEntity.GetLive();
    }

    SP_EXPORT void game_send_input_bool(StrayPhotons ctx, uint64_t input_device, const char *event_name, int value) {
        Assertf(ctx != nullptr, "sp::game_send_input_bool called with null ctx");
        if (ctx->disableInput) return;
        ctx->game.inputEventQueue.PushEvent(ecs::Event{event_name, input_device, (bool)value});
    }

    SP_EXPORT void game_send_input_str(StrayPhotons ctx,
        uint64_t input_device,
        const char *event_name,
        const char *value) {
        Assertf(ctx != nullptr, "sp::game_send_input_str called with null ctx");
        if (ctx->disableInput) return;
        ctx->game.inputEventQueue.PushEvent(ecs::Event{event_name, input_device, std::string(value)});
    }

    SP_EXPORT void game_send_input_int(StrayPhotons ctx, uint64_t input_device, const char *event_name, int value) {
        Assertf(ctx != nullptr, "sp::game_send_input_int called with null ctx");
        if (ctx->disableInput) return;
        ctx->game.inputEventQueue.PushEvent(ecs::Event{event_name, input_device, value});
    }

    SP_EXPORT void game_send_input_uint(StrayPhotons ctx,
        uint64_t input_device,
        const char *event_name,
        unsigned int value) {
        Assertf(ctx != nullptr, "sp::game_send_input_uint called with null ctx");
        if (ctx->disableInput) return;
        ctx->game.inputEventQueue.PushEvent(ecs::Event{event_name, input_device, value});
    }

    SP_EXPORT void game_send_input_vec2(StrayPhotons ctx,
        uint64_t input_device,
        const char *event_name,
        float value_x,
        float value_y) {
        Assertf(ctx != nullptr, "sp::game_send_input_vec2 called with null ctx");
        if (ctx->disableInput) return;
        ctx->game.inputEventQueue.PushEvent(ecs::Event{event_name, input_device, glm::vec2(value_x, value_y)});
    }
} // namespace sp
