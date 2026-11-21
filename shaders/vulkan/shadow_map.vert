/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justine Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#version 460
#extension GL_EXT_shader_16bit_storage : require

#include "../lib/types_common.glsl"
#include "../lib/util.glsl"
#include "../lib/vertex_base.glsl"

layout(location = 0) in vec3 inPos;
layout(location = 0) out vec3 outViewPos;
layout(location = 1) flat out uint outRenderableIndex;

#include "lib/draw_params.glsl"

layout(binding = 0) uniform LightViews {
    ViewState lightViews[];
};

layout(std430, set = 1, binding = 0) readonly buffer DrawParamsList {
    DrawParams drawParams[];
};

void main() {
    outViewPos = vec3(lightViews[0].viewMat * vec4(inPos, 1.0));
    outRenderableIndex = uint(drawParams[gl_BaseInstance].renderableIndex);
    gl_Position = lightViews[0].projMat * vec4(outViewPos, 1.0);
}
