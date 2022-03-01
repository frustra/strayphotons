#ifndef UTIL_GLSL_INCLUDED
#define UTIL_GLSL_INCLUDED

#define INCLUDE_LAYOUT layout

#define M_PI 3.1415926535897932384626433832795

#define saturate(x) clamp(x, 0.0, 1.0)
#define hash(x) fract(sin(x) * 43758.543123)

float rand2(inout vec4 state) {
    // Hachisuka 2015
    const vec4 q = vec4(1225.0, 1585.0, 2457.0, 2098.0);
    const vec4 r = vec4(1112.0, 367.0, 92.0, 265.0);
    const vec4 a = vec4(3423.0, 2646.0, 1707.0, 1999.0);
    const vec4 m = vec4(4194287.0, 4194277.0, 4194191.0, 4194167.0);

    vec4 b = floor(state / q);
    vec4 p = a * (state - b * q) - b * r;
    b = (sign(-p) + vec4(1.0)) * vec4(0.5) * m;
    state = p + b;

    return fract(dot(state / m, vec4(1.0, -1.0, 1.0, -1.0)));
}

vec4 randState(vec3 seed) {
    vec4 rng;
    rng.x = hash(seed.x);
    rng.y = hash(seed.y + rng.x);
    rng.z = hash(seed.z + rng.x + rng.y);
    rng.w = hash(dot(vec3(1.0), rng.xyz));
    rand2(rng);
    return rng;
}

float linstep(float low, float high, float v) {
    return clamp((v - low) / (high - low), 0.0, 1.0);
}

#include "color_util.glsl"
#include "spatial_util.glsl"

#endif
