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

extern "C" {
extern void print_str(const char *);

void onTick(Entity e) {
    // printf("Hello World from WASM! Entity: %u", e);
    char messageA[] = "Hello World from WASM! Entity: ";
    char messageB[] = "Hello World from WASM! Entity: \0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";
    unsigned int value = (unsigned int)e;
    int i = sizeof(messageA) - 1;
    while (value > 0 && i < sizeof(messageB)) {
        messageB[i++] = value % 10 + '0';
        value /= 10;
    }
    if (i == sizeof(messageB)) {
        messageB[--i] = '\0';
        while (i >= sizeof(messageA)) {
            messageB[--i] = 'E';
        }
    } else {
        for (int j = sizeof(messageA) - 1; i > j; i--, j++) {
            char tmp = messageB[j];
            messageB[j] = messageB[i - 1];
            messageB[i - 1] = tmp;
        }
    }
    print_str(messageB);
}
}
