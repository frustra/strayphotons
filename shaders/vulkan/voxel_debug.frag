/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#version 450

#extension GL_OVR_multiview2 : enable
layout(num_views = 2) in;

#include "../lib/types_common.glsl"
#include "../lib/util.glsl"

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outFragColor;

INCLUDE_LAYOUT(binding = 0)
#include "lib/view_states_uniform.glsl"

layout(binding = 1) uniform VoxelStateUniform {
    VoxelState voxelInfo;
};

INCLUDE_LAYOUT(binding = 2)
#include "lib/exposure_state.glsl"

layout(binding = 3) uniform sampler2DArray overlayTex;
// layout(binding = 4, r32ui) readonly uniform uimage3D fillCounters;
layout(std430, binding = 4) readonly buffer FillCounters {
    uint fillCounters[];
};
layout(binding = 5) uniform sampler3D voxelRadiance;
layout(binding = 6) uniform sampler3D voxelNormals;

layout(set = 1, binding = 0) uniform sampler3D voxelLayers[6];

layout(constant_id = 0) const int DEBUG_MODE = 1;
layout(constant_id = 1) const float BLEND_WEIGHT = 0;
layout(constant_id = 2) const int VOXEL_MIP = 0;

#include "../lib/voxel_shared.glsl"
#include "../lib/voxel_trace_debug.glsl"
#include "../lib/voxel_trace_shared.glsl"

void main() {
    ViewState view = views[gl_ViewID_OVR];

    vec2 flippedCoord = vec2(inTexCoord.x, 1 - inTexCoord.y);
    vec4 rayPos = view.invViewMat * vec4(ScreenPosToViewPos(flippedCoord, 0, view.invProjMat), 1);
    vec3 rayDir = normalize(rayPos.xyz - view.invViewMat[3].xyz);

    vec3 sampleRadiance;
    if (DEBUG_MODE == 1) {
        TraceVoxelGrid(rayPos.xyz, rayDir, VOXEL_MIP, sampleRadiance);
    } else if (DEBUG_MODE == 2) {
        sampleRadiance = ConeTraceGrid(1 / 50.0, rayPos.xyz, rayDir.xyz, rayDir.xyz, gl_FragCoord.xy).rgb;
    } else if (DEBUG_MODE == 3) {
        sampleRadiance = ConeTraceGridDiffuse(rayPos.xyz, rayDir.xyz, InterleavedGradientNoise(gl_FragCoord.xy)).rgb;
    } else if (DEBUG_MODE == 4) {
        TraceVoxelGrid2(rayPos.xyz, rayDir, sampleRadiance);
    }

    vec3 overlay = texture(overlayTex, vec3(inTexCoord, gl_ViewID_OVR)).rgb; // pre-exposed
    outFragColor = vec4(mix(sampleRadiance * exposure, overlay, BLEND_WEIGHT), 1.0);
}
