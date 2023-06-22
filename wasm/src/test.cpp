/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <cstdio>
#include <ecs.h>
#include <ecs/components/Transform.h>
#include <string>

// If this changes, make sure it is the same in C++ and Rust
static_assert(sizeof(ecs::Transform) == 60, "Wrong Transform size");

int add(int a, int b) {
    return a + b;
}

extern "C" void print_str(const char *);

extern "C" void onTick(Entity e) {
    char message[64];
    sprintf(&message[0], "Hello World from WASM! Entity: %llu", e);
    print_str(message);

    printf("Hello World POSIX! %llu\n", e);
}
