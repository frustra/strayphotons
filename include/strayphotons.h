/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
namespace sp {
    struct CGameContext;

    extern "C" {
    typedef CGameContext *StrayPhotons;
    static_assert(sizeof(uintptr_t) <= sizeof(uint64_t), "Pointer size larger than handle");
#else
typedef uint64_t StrayPhotons;
#endif

    StrayPhotons game_init(int argc, char **argv);
    int game_start(StrayPhotons ctx);
    void game_destroy(StrayPhotons ctx);

#ifdef __cplusplus
    }
}
#endif
