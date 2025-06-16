/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#version 450

#extension GL_OVR_multiview2 : enable
layout(num_views = 2) in;

#include "../lib/types_common.glsl"
#include "../lib/util.glsl"

INCLUDE_LAYOUT(binding = 1)
#include "lib/view_states_uniform.glsl"

layout(location = 0) in float inFrontFace;
layout(location = 0) out vec4 outFragColor;

void main() {
    ViewState view = views[gl_ViewID_OVR];
    float viewDist = length(view.invViewMat[3].xyz);

    outFragColor = vec4(vec3(max(0, inFrontFace * viewDist * 0.01)), 1.0);
}
