/*
 * Stray Photons - Copyright (C) 2026 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#version 460
#extension GL_EXT_shader_16bit_storage : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require
#extension GL_EXT_shader_explicit_arithmetic_types_float16 : require
#extension GL_OVR_multiview2 : enable
layout(num_views = 2) in;

#include "../lib/types_common.glsl"
#include "../lib/util.glsl"
#include "../lib/vertex_base.glsl"

layout(location = 0) in vec4 inPosition;

layout(location = 0) out vec3 outViewPos;

INCLUDE_LAYOUT(binding = 0)
#include "lib/view_states_uniform.glsl"

void main() {
    ViewState view = views[gl_ViewID_OVR];
    vec4 viewPos4 = view.viewMat * inPosition;
    if (viewPos4 == vec4(0)) {
        outViewPos = vec3(0, 0, 1);
        gl_Position = vec4(0);
    } else {
        outViewPos = vec3(viewPos4) / viewPos4.w;
        gl_Position = view.projMat * viewPos4;
    }
    gl_PointSize = 4.0f;
}
