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
    float depth = texture(gBufferDepth, vec3(screenPos, gl_ViewID_OVR)).r;
    vec3 viewPosition = ScreenPosToViewPos(screenPos, depth, view.invProjMat);
    vec3 worldPosition = (view.invViewMat * vec4(viewPosition, 1.0)).xyz;

	float rand = mod(smoothstep(0.0, 1.0, InterleavedGradientNoise(gl_FragCoord.xy)) - colorTime.w * 0.5, 1.0);
	float rand2 = mod(rand + 0.5, 1.0);

	vec2 outPosition = ViewPosToScreenPos((view.viewMat * vec4(worldPosition, 1.0)).xyz, view.projMat).xy;
	vec2 centerDir = vec2(0.5, 1.05) - outPosition;
	imageStore(imageOut, ivec3((outPosition + centerDir * rand) * size, gl_ViewID_OVR), vec4(colorTime.rgb, 1));
	imageStore(imageOut, ivec3((outPosition + centerDir * rand2) * size, gl_ViewID_OVR), vec4(colorTime.rgb, 1));
}
