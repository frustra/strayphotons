/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "export.h"

#ifdef __cplusplus
namespace sp {
    struct CGameContext;
}
extern "C" {
typedef sp::CGameContext sp_game_t;
#else
typedef void sp_game_t;
#endif

#include <assert.h>
#include <stdbool.h>

static_assert(sizeof(bool) == 1, "Unexpected bool size");

struct GLFWwindow;

// The following functions are declared in src/exports/Game.cc

SP_EXPORT sp_game_t *sp_game_init(int argc, char **argv);
SP_EXPORT bool sp_game_get_cli_flag(sp_game_t *ctx, const char *arg_name);
SP_EXPORT int sp_game_start(sp_game_t *ctx);
SP_EXPORT void sp_game_trigger_exit(sp_game_t *ctx);
SP_EXPORT bool sp_game_is_exit_triggered(sp_game_t *ctx);
SP_EXPORT int sp_game_wait_for_exit_trigger(sp_game_t *ctx);
SP_EXPORT int sp_game_get_exit_code(sp_game_t *ctx);
SP_EXPORT void sp_game_destroy(sp_game_t *ctx);

#ifdef __cplusplus
}
#endif
