/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <sp_export.h>
#include <stdint.h>

#ifdef __cplusplus
namespace ecs {
    struct ECSContext;
    class ScriptManager;
    struct ScriptDefinitions;
} // namespace ecs

namespace sp {
    struct CGameContext;
    class ConsoleManager;
    class AssetManager;
    class SceneManager;

    extern "C" {
    typedef CGameContext *StrayPhotons;
    static_assert(sizeof(uintptr_t) <= sizeof(uint64_t), "Pointer size larger than handle");

    using namespace ecs;
#else
typedef uint64_t StrayPhotons;
typedef void ECSContext;
typedef void ConsoleManager;
typedef void ScriptManager;
typedef void ScriptDefinitions;
typedef void AssetManager;
typedef void SceneManager;
#endif

    // The following functions are declared in StrayPhotons.cc

    SP_EXPORT StrayPhotons game_init(int argc, char **argv);
    SP_EXPORT void game_set_shutdown_callback(StrayPhotons ctx, void (*callback)(StrayPhotons));
    SP_EXPORT int game_start(StrayPhotons ctx);
    SP_EXPORT void game_destroy(StrayPhotons ctx);

    SP_EXPORT ConsoleManager *game_get_console_manager();
    SP_EXPORT ECSContext *game_get_ecs_context();
    SP_EXPORT ScriptManager *game_get_script_manager();
    SP_EXPORT ScriptDefinitions *game_get_script_definitons();
    SP_EXPORT AssetManager *game_get_asset_manager();
    SP_EXPORT SceneManager *game_get_scene_manager();

    SP_EXPORT uint64_t game_new_input_device(StrayPhotons ctx, const char *name);
    SP_EXPORT void game_send_input_bool(StrayPhotons ctx, uint64_t input_device, const char *event_name, int value);
    SP_EXPORT void game_send_input_str(StrayPhotons ctx,
        uint64_t input_device,
        const char *event_name,
        const char *value);
    SP_EXPORT void game_send_input_int(StrayPhotons ctx, uint64_t input_device, const char *event_name, int value);
    SP_EXPORT void game_send_input_uint(StrayPhotons ctx,
        uint64_t input_device,
        const char *event_name,
        unsigned int value);
    SP_EXPORT void game_send_input_vec2(StrayPhotons ctx,
        uint64_t input_device,
        const char *event_name,
        float value_x,
        float value_y);

#ifdef __cplusplus
    }
}
#endif
