/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#version 460
#extension GL_EXT_shader_16bit_storage : require
#extension GL_ARB_shader_viewport_layer_array : enable

#include "../lib/types_common.glsl"
#include "../lib/util.glsl"
#include "../lib/vertex_base.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outVoxelPos;
layout(location = 2) out vec3 outNormal;
layout(location = 3) out vec2 outTexCoord;
layout(location = 4) flat out int baseColorTexID;
layout(location = 5) flat out int metallicRoughnessTexID;
layout(location = 6) flat out float emissiveScale;

#include "lib/draw_params.glsl"
layout(std430, set = 1, binding = 0) readonly buffer DrawParamsList {
    DrawParams drawParams[];
};

layout(binding = 0) uniform ViewStates {
    ViewState views[3];
};

layout(binding = 1) uniform VoxelStateUniform {
    VoxelState voxelInfo;
};

void main() {
    outWorldPos = inPosition;

    ViewState view = views[gl_InstanceIndex - gl_BaseInstance];
    gl_Position = view.viewMat * vec4(inPosition, 1.0);

    outVoxelPos = (voxelInfo.worldToVoxel * vec4(inPosition, 1.0)).xyz;
    outNormal = normalize(inNormal);
    outTexCoord = inTexCoord;

    baseColorTexID = int(drawParams[gl_BaseInstance].baseColorTexID);
    metallicRoughnessTexID = int(drawParams[gl_BaseInstance].metallicRoughnessTexID);
    emissiveScale = float(drawParams[gl_BaseInstance].emissiveScale);

    gl_ViewportIndex = int(gl_InstanceIndex - gl_BaseInstance);
}
