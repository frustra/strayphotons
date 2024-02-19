/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "export.h"

#include <stdint.h>

#ifdef __cplusplus
namespace cxxopts {
    class ParseResult;
}

namespace ecs {
    struct ECSContext;
    class SignalManager;
    class ScriptManager;
    struct ScriptDefinitions;
} // namespace ecs

namespace sp {
    struct CGameContext;
    class ConsoleManager;
    class AssetManager;
    class SceneManager;
    class GraphicsContext;
} // namespace sp

extern "C" {
using namespace cxxopts;
using namespace sp;
using namespace ecs;

typedef CGameContext *sp_game_t;
static_assert(sizeof(sp_game_t) == sizeof(uint64_t), "Pointer size doesn't match handle");
#else
typedef uint64_t sp_game_t;
typedef void ParseResult;
typedef void ECSContext;
typedef void ConsoleManager;
typedef void SignalManager;
typedef void ScriptManager;
typedef void ScriptDefinitions;
typedef void AssetManager;
typedef void SceneManager;
typedef void GraphicsContext;
#endif

// The following functions are declared in src/exports/Game.cc

SP_EXPORT sp_game_t sp_game_init(int argc, char **argv);
SP_EXPORT void sp_game_set_shutdown_callback(sp_game_t game, void (*callback)(sp_game_t));
SP_EXPORT GraphicsContext *sp_game_get_graphics_context(sp_game_t game);
SP_EXPORT ParseResult *sp_game_get_options(sp_game_t ctx);
SP_EXPORT int sp_game_start(sp_game_t game);
SP_EXPORT void sp_game_destroy(sp_game_t game);

SP_EXPORT ConsoleManager *sp_get_console_manager();
SP_EXPORT ECSContext *sp_get_ecs_context();
SP_EXPORT SignalManager *sp_get_signal_manager();
SP_EXPORT ScriptManager *sp_get_script_manager();
SP_EXPORT ScriptDefinitions *sp_get_script_definitons();
SP_EXPORT AssetManager *sp_get_asset_manager();
SP_EXPORT SceneManager *sp_get_scene_manager();

#ifdef __cplusplus
}
#endif
