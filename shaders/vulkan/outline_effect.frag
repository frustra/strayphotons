/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#version 450

#extension GL_OVR_multiview2 : enable
layout(num_views = 2) in;
layout(early_fragment_tests) in;

#include "../lib/types_common.glsl"
#include "../lib/util.glsl"

layout(binding = 0, rgba16f) writeonly uniform image2DArray imageOut;
layout(binding = 1) uniform sampler2DArray gBufferDepth;

INCLUDE_LAYOUT(binding = 10)
#include "lib/view_states_uniform.glsl"

layout(location = 0) in vec3 inViewPos;

layout(push_constant) uniform PushConstants {
    vec4 colorTime;
};

void main() {
    ViewState view = views[gl_ViewID_OVR];
    vec2 size = imageSize(imageOut).xy;
    vec2 screenPos = gl_FragCoord.xy / size;
    float targetDepth = gl_FragCoord.z * 0.5 + 0.5;
    float sampledDepth = texture(gBufferDepth, vec3(screenPos, gl_ViewID_OVR)).r;
    // targetDepth = sampledDepth;

    vec3 viewPosition = ScreenPosToViewPos(screenPos, targetDepth, view.invProjMat);
    vec3 worldPosition = (view.invViewMat * vec4(viewPosition, 1.0)).xyz;
    vec2 outPosition = ViewPosToScreenPos((view.viewMat * vec4(worldPosition, 1.0)).xyz, view.projMat).xy;
    vec3 viewPosition2 = ScreenPosToViewPos(screenPos, targetDepth, view.invProjMat);
    viewPosition2.xy *= 2 * sqrt(-viewPosition2.z);
    // viewPosition2.y = viewPosition2.y * 0.5 - 0.5;
    vec3 worldPosition2 = (view.invViewMat * vec4(viewPosition2, 1.0)).xyz;
    vec2 edgePosition = ViewPosToScreenPos((view.viewMat * vec4(worldPosition2, 1.0)).xyz, view.projMat).xy;

    // vec2 viewDir = normalize(viewPosition.xy);
    // float angle = atan(viewDir.y, viewDir.x) + M_PI;
    // float angle = length(worldPosition - worldPosition);
    vec4 rng = randState(gl_FragCoord.xyx);
    float noise = rand2(rng); // mod(gl_FragCoord.y, 1.5);//InterleavedGradientNoise(gl_FragCoord.xy);
    // float rand = 1 - sqrt(mod(smoothstep(0.0, 1.0, noise) + colorTime.w * 0.5, 1.0));
    float rand = 0.5 - sqrt(mod(smoothstep(0.0, 1.0, noise) + colorTime.w * 0.125, 0.25));
    // float rand = 1 - sqrt(mod(noise + colorTime.w * 0.5, 1.0));
    // float rand = 0.5 - sqrt(mod(noise + colorTime.w * 0.5, 0.25));
    // float rand2 =  1 - sqrt(mod(smoothstep(0.0, 1.0, noise) + colorTime.w * 0.5 + 0.5, 1.0));

    // edgePosition.x = smoothstep(0.25, 0.75, edgePosition.x);
    // edgePosition.x = clamp(edgePosition.x, 0.2, 0.8);
    edgePosition.y = clamp(edgePosition.y, 0.0, 0.6) - 0.6;
    // edgePosition.y = -0.4;
    vec2 edgePosition2 = edgePosition;
    // edgePosition.x = step(0.5, edgePosition.x) * 0.6 + 0.2;
    // edgePosition2.x = step(0.5, edgePosition2.x) * 0.2 + 0.4;
    // edgePosition.x = (edgePosition.x * 0.5 - 0.5) * 0.75 * 2.0 + 1.0;
    // edgePosition.x = clamp(floor(edgePosition.x * 5), 1, 4) / 5.0;
    edgePosition.x = clamp(edgePosition.x, 0.4, 0.6);
    // edgePosition2.x = floor(edgePosition2.x * 3) / 5.0 + 2.0 / 7.0;
    // vec2 delta = vec2(smoothstep(0.0, 1.0, edgePosition)) - outPosition;
    vec2 delta = edgePosition - outPosition;
    vec2 delta2 = edgePosition2 - outPosition;
    vec3 startPos = vec3((outPosition + delta * rand) * size, gl_ViewID_OVR);
    imageStore(imageOut, ivec3((outPosition + delta * rand) * size, gl_ViewID_OVR), vec4(colorTime.rgb, 1));
    // imageStore(imageOut, ivec3((outPosition + delta2 * rand) * size, gl_ViewID_OVR), vec4(colorTime.rgb, 1));
}
