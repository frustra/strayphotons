/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "ecs/Ecs.hh"

#include <strayphotons/ecs.h>

SP_EXPORT TecsECS *sp_get_live_ecs() {
    return &ecs::World();
}

SP_EXPORT TecsECS *sp_get_staging_ecs() {
    return &ecs::StagingWorld();
}
