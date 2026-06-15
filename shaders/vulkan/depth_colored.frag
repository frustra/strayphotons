/*
 * Stray Photons - Copyright (C) 2026 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#version 450

#extension GL_OVR_multiview2 : enable
layout(num_views = 2) in;

#include "../lib/perlin.glsl"
#include "../lib/types_common.glsl"
#include "../lib/util.glsl"

INCLUDE_LAYOUT(binding = 0)
#include "lib/view_states_uniform.glsl"

INCLUDE_LAYOUT(binding = 1)
#include "lib/exposure_state.glsl"

layout(location = 0) in vec3 inViewPos;
layout(location = 0) out vec4 outFragColor;

void main() {
    ViewState view = views[gl_ViewID_OVR];

    // if (inViewPos.z > 0) discard;
    vec3 worldPosition = (view.invViewMat * vec4(inViewPos, 1.0)).xyz;
    float noise = PerlinNoise3D(worldPosition * 10) * 0.1;
    outFragColor = vec4(vec3(abs(inViewPos.z * 0.1 + noise) * exposure), 1.0);
}
