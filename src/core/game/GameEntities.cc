/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "GameEntities.hh"

#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"

namespace sp::entities {

    const ecs::EntityRef Spawn = ecs::Name("global", "spawn");
    const ecs::EntityRef Player = ecs::Name("player", "player");
    const ecs::EntityRef Direction = ecs::Name("player", "direction");
    const ecs::EntityRef Pointer = ecs::Name("player", "pointer");
    const ecs::EntityRef VrPointer = ecs::Name("vr", "pointer");
    const ecs::EntityRef Head = ecs::Name("player", "head");
    const ecs::EntityRef FlatHead = ecs::Name("player", "flathead");
    const ecs::EntityRef VrHmd = ecs::Name("vr", "hmd");

} // namespace sp::entities
