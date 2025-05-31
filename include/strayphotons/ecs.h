/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "export.h"

#include <c_abi/Tecs.h>

#ifdef __cplusplus
extern "C" {
#endif

// The following functions are declared in src/exports/Ecs.cc

SP_EXPORT tecs_ecs_t *sp_get_live_ecs();
SP_EXPORT tecs_ecs_t *sp_get_staging_ecs();

#ifdef __cplusplus
}
#endif
