/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <stdio.h>
#include <strayphotons.h>

int main(int argc, char **argv) {
    printf("sp-winit starting...\n");
    StrayPhotons instance = game_init(argc, argv);
    if (!instance) return 1;
    int result = game_start(instance);
    game_destroy(instance);
    return result;
}
