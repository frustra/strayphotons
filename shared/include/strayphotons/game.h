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
#endif

struct GLFWwindow;

// The following functions are declared in src/exports/Game.cc

SP_EXPORT sp_game_t sp_game_init(int argc, char **argv);
SP_EXPORT int sp_game_start(sp_game_t game);
SP_EXPORT void sp_game_trigger_exit(sp_game_t game);
SP_EXPORT bool sp_game_is_exit_triggered(sp_game_t game);
SP_EXPORT int sp_game_wait_for_exit_trigger(sp_game_t game);
SP_EXPORT int sp_game_get_exit_code(sp_game_t game);
SP_EXPORT void sp_game_destroy(sp_game_t game);

SP_EXPORT ParseResult *sp_game_get_options(sp_game_t ctx);

SP_EXPORT void sp_game_set_input_handler(sp_game_t ctx, void *handler, void (*destroy_callback)(void *));
SP_EXPORT void *sp_game_get_input_handler(sp_game_t ctx);

#ifdef __cplusplus
}
#endif
