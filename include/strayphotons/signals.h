/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "export.h"
#include "game.h"

#ifdef __cplusplus
namespace ecs {
    class SignalRef;
}

extern "C" {
typedef ecs::SignalRef *sp_signal_ref_t;
static_assert(sizeof(sp_signal_ref_t) == sizeof(uint64_t), "Pointer size doesn't match handle");
#else
typedef uint64_t sp_signal_ref_t;
#endif

// The following functions are declared in src/exports/Signals.cc

SP_EXPORT sp_signal_ref_t sp_get_signal_ref(sp_game_t ctx, sp_signal_ref_t entity, const char *signal_name);
// SP_EXPORT float sp_get_signal(sp_game_t ctx, sp_signal_ref_t signal_ref);
// SP_EXPORT void sp_set_signal(sp_game_t ctx, sp_signal_ref_t signal_ref, float value);

#ifdef __cplusplus
}
#endif
