#ifndef MEDIA_DENSITY_GLSL_INCLUDED
#define MEDIA_DENSITY_GLSL_INCLUDED

#include "perlin.glsl"

float MediaDensity(vec3 worldPos, float time) {
    float macro = PerlinNoise3D(worldPos.xyz * 3 + time * 0.3);
    float micro = PerlinNoise3D(worldPos.xyz * 10 - time * 0.4);
    return macro * 0.5 - micro * 0.2 + 0.5;
}

#endif
