/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/EntityRef.hh"

namespace sp::entities {

    extern const ecs::EntityRef Spawn;
    extern const ecs::EntityRef Player;
    extern const ecs::EntityRef Direction;
    extern const ecs::EntityRef Pointer;
    extern const ecs::EntityRef VrPointer;
    extern const ecs::EntityRef Head;
    extern const ecs::EntityRef FlatHead;
    extern const ecs::EntityRef VrHmd;

} // namespace sp::entities
