/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#version 450

#extension GL_OVR_multiview2 : enable
layout(num_views = 2) in;

#include "../lib/media_density.glsl"
#include "../lib/types_common.glsl"
#include "../lib/util.glsl"

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in vec3 inWorldPos;
layout(location = 2) in float inScale;
layout(location = 0) out vec4 outFragColor;

INCLUDE_LAYOUT(binding = 1)
#include "lib/exposure_state.glsl"

layout(push_constant) uniform PushConstants {
    vec3 radiance;
    float radius;
    vec3 start;
    float mediaDensityFactor;
    vec3 end;
    float time;
};

void main() {
    float dist = abs(inTexCoord.x - 0.5);

    float weight = (1 - smoothstep(0.2, 0.48, dist)) * inScale;

    if (mediaDensityFactor > 0) {
        float density = MediaDensity(inWorldPos, time);
        density = mediaDensityFactor * (density - 1) + 1;
        weight *= pow(density, 4);
    }

    outFragColor.rgb = radiance * exposure;
    outFragColor.a = max(weight, 0);
}
