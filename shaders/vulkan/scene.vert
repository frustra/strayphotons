/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
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

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 outViewPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outTexCoord;
layout(location = 3) flat out int baseColorTexID;
layout(location = 4) flat out int metallicRoughnessTexID;
layout(location = 5) flat out float emissiveScale;

#include "lib/draw_params.glsl"
layout(std430, set = 1, binding = 0) readonly buffer DrawParamsList {
    DrawParams drawParams[];
};

INCLUDE_LAYOUT(binding = 10)
#include "lib/view_states_uniform.glsl"

void main() {
    ViewState view = views[gl_ViewID_OVR];
    vec4 viewPos4 = view.viewMat * vec4(inPosition, 1.0);
    outViewPos = vec3(viewPos4) / viewPos4.w;
    gl_Position = view.projMat * viewPos4;

    mat3 rotation = mat3(view.viewMat);
    outNormal = rotation * inNormal;
    outTexCoord = inTexCoord;

    baseColorTexID = int(drawParams[gl_BaseInstance].baseColorTexID);
    metallicRoughnessTexID = int(drawParams[gl_BaseInstance].metallicRoughnessTexID);
    emissiveScale = float(drawParams[gl_BaseInstance].emissiveScale);
}
