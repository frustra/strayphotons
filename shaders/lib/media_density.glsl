/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef MEDIA_DENSITY_GLSL_INCLUDED
#define MEDIA_DENSITY_GLSL_INCLUDED

#include "perlin.glsl"

float MediaDensity(vec3 worldPos, float time) {
    float macro = PerlinNoise3D(worldPos.xyz * 3 + time * 0.3);
    float micro = PerlinNoise3D(worldPos.xyz * 10 - time * 0.4);
    return macro * 0.5 - micro * 0.2 + 0.5;
}

#endif
