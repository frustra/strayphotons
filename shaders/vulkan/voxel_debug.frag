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

layout(binding = 3, r32ui) readonly uniform uimage3D fillCounters;
layout(binding = 4) uniform sampler3D voxelRadiance;
layout(binding = 5) uniform sampler2DArray overlayTex;

layout(constant_id = 0) const float BLEND_WEIGHT = 0;

#include "../lib/voxel_trace_debug.glsl"
#include "../lib/voxel_trace_shared.glsl"

void main() {
    ViewState view = views[gl_ViewID_OVR];

    vec4 rayPos = view.invViewMat *
                  vec4(ScreenPosToViewPos(vec2(inTexCoord.x, 1 - inTexCoord.y), 0, view.invProjMat), 1);
    vec3 rayDir = normalize(rayPos.xyz - view.invViewMat[3].xyz);

    vec3 sampleRadiance;
    float count = TraceVoxelGrid(rayPos.xyz, rayDir, 0, sampleRadiance);
    // sampleRadiance = ConeTraceGrid(1 / 50.0, rayPos.xyz, rayDir.xyz, rayDir.xyz, gl_FragCoord.xy).rgb;
    // sampleRadiance = ConeTraceGridDiffuse(rayPos.xyz, rayDir.xyz, 0).rgb;
    // sampleRadiance = vec3(1 / count);

    vec3 overlay = texture(overlayTex, vec3(inTexCoord, gl_ViewID_OVR)).rgb; // pre-exposed
    outFragColor = vec4(mix(sampleRadiance * exposure, overlay, BLEND_WEIGHT), 1.0);
}
