/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "export.h"

#ifdef __cplusplus
    #include <cstdint>

namespace sp {
    class CVarBase;
} // namespace sp

extern "C" {
typedef sp::CVarBase sp_cvar_t;
#else
    #include <stdint.h>

typedef void sp_cvar_t;
typedef unsigned int bool;
#endif

// The following functions are declared in src/exports/Console.cc

SP_EXPORT sp_cvar_t *sp_get_cvar(const char *name);

SP_EXPORT bool sp_cvar_get_bool(sp_cvar_t *cvar);
SP_EXPORT void sp_cvar_set_bool(sp_cvar_t *cvar, bool value);

SP_EXPORT float sp_cvar_get_float(sp_cvar_t *cvar);
SP_EXPORT void sp_cvar_set_float(sp_cvar_t *cvar, float value);

SP_EXPORT uint32_t sp_cvar_get_uint32(sp_cvar_t *cvar);
SP_EXPORT void sp_cvar_set_uint32(sp_cvar_t *cvar, uint32_t value);

SP_EXPORT void sp_cvar_get_ivec2(sp_cvar_t *cvar, int *out_x, int *out_y);
SP_EXPORT void sp_cvar_set_ivec2(sp_cvar_t *cvar, int value_x, int value_y);

SP_EXPORT void sp_cvar_get_vec2(sp_cvar_t *cvar, float *out_x, float *out_y);
SP_EXPORT void sp_cvar_set_vec2(sp_cvar_t *cvar, float value_x, float value_y);

SP_EXPORT sp_cvar_t *sp_register_cfunc_uint32(const char *name,
    const char *description,
    void (*cfunc_callback)(uint32_t));
SP_EXPORT void sp_unregister_cfunc(sp_cvar_t *cfunc);

#ifdef __cplusplus
}
#endif
