/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <ecs.h>
#include <ecs/components/Transform.h>

// If this changes, make sure it is the same in C++ and Rust
static_assert(sizeof(ecs::Transform) == 60, "Wrong Transform size");

int add(int a, int b) {
    return a + b;
}

void onTick(Entity e) {}

void onSignalChange(Entity e, const char *signal_name, double new_value) {}
