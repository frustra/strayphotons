/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "export.h"
#include "game.h"

#include <stdint.h>

#ifdef __cplusplus
namespace ecs {
    class EntityRef;
}

extern "C" {
typedef uint64_t sp_entity_t;
// typedef ecs::EntityRef *sp_entity_ref_t;
// static_assert(sizeof(sp_entity_ref_t) == sizeof(uint64_t), "Pointer size doesn't match handle");
#else
typedef uint64_t sp_entity_t;
// typedef uint64_t sp_entity_ref_t;
#endif

// The following functions are declared in src/exports/Entity.cc

// SP_EXPORT sp_entity_ref_t sp_new_entity_ref(const char *scene, const char *name);
// SP_EXPORT sp_entity_ref_t sp_get_entity_ref(sp_entity_t entity);
// SP_EXPORT void sp_drop_entity_ref(sp_entity_ref_t entity_ref);

#ifdef __cplusplus
}
#endif
