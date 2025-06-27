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

layout(binding = 0) uniform sampler2DArray tex;
layout(binding = 1) uniform sampler2DArray gBufferDepth;

INCLUDE_LAYOUT(binding = 2)
#include "lib/view_states_uniform.glsl"

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform PushConstants {
	float transmittance;
};

void main() {
    ViewState view = views[gl_ViewID_OVR];
    float depth = texture(gBufferDepth, vec3(inTexCoord, gl_ViewID_OVR)).r;
    vec2 screenPos = vec2(inTexCoord.x, 1 - inTexCoord.y);
    vec3 viewPosition = ScreenPosToViewPos(screenPos, depth, view.invProjMat);
    vec3 worldPosition = (view.invViewMat * vec4(viewPosition, 1.0)).xyz;

	float value = texture(tex, vec3(inTexCoord, gl_ViewID_OVR)).r;
	if (value < 0) {
		value += length(worldPosition - view.invViewMat[3].xyz) * transmittance;
	}
	// value = length(worldPosition - view.invViewMat[3].xyz) - length(viewPosition);
    outFragColor = vec4(vec3(abs(value), vec2(max(0, value))), 1.0);
}
