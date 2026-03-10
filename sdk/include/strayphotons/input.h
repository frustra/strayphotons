/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "entity.h"
#include "export.h"
#include "game.h"

#ifdef __cplusplus
extern "C" {
#endif

// The following functions are declared in src/exports/Input.cc

SP_EXPORT sp_entity_t sp_new_input_device(sp_game_t *ctx, const char *name);
SP_EXPORT void sp_send_input_bool(sp_game_t *ctx, sp_entity_t input_device, const char *event_name, int value);
SP_EXPORT void sp_send_input_str(sp_game_t *ctx, sp_entity_t input_device, const char *event_name, const char *value);
SP_EXPORT void sp_send_input_int(sp_game_t *ctx, sp_entity_t input_device, const char *event_name, int value);
SP_EXPORT void sp_send_input_uint(sp_game_t *ctx, sp_entity_t input_device, const char *event_name, unsigned int value);
SP_EXPORT void sp_send_input_vec2(sp_game_t *ctx,
    sp_entity_t input_device,
    const char *event_name,
    float value_x,
    float value_y);

#ifdef __cplusplus
}
#endif
