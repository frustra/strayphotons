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

INCLUDE_LAYOUT(binding = 2)
#include "lib/light_data_uniform.glsl"

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in float inFrontFace;
layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform PushConstants {
    uint lightIndex;
	float transmittance;
};

void main() {
    ViewState view = views[gl_ViewID_OVR];
	vec3 viewWorldPosition = view.invViewMat[3].xyz;

    vec3 viewDir = normalize(inWorldPos - viewWorldPosition);

	vec3 lightWorldPosition = lights[lightIndex].invView[3].xyz;
	vec3 lightDir = normalize(lightWorldPosition - viewWorldPosition);

    float viewDist = length(inWorldPos - viewWorldPosition) * transmittance;

    outFragColor = vec4(-inFrontFace * viewDist);
}
