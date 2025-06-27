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

layout(binding = 0) uniform sampler2D shadowMap;

INCLUDE_LAYOUT(binding = 1)
#include "lib/view_states_uniform.glsl"

INCLUDE_LAYOUT(binding = 2)
#include "lib/light_data_uniform.glsl"

layout(push_constant) uniform PushConstants {
    uint lightIndex;
};

out gl_PerVertex {
    vec4 gl_Position;
};

const vec2 squares[12] = vec2[](
    // Front face
    vec2(0, 0),
    vec2(1, 1),
    vec2(1, 0),
    vec2(0, 0),
    vec2(0, 1),
    vec2(1, 1),
    // Back face
    vec2(0, 0),
    vec2(1, 0),
    vec2(1, 1),
    vec2(0, 0),
    vec2(1, 1),
    vec2(0, 1));

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) flat out float outFrontFace;

void main() {
    ViewState view = views[gl_ViewID_OVR];

    ivec2 mapSize = ivec2(textureSize(shadowMap, 0).xy * lights[lightIndex].mapOffset.zw);

    int pixelIndex = gl_VertexIndex / 12;
    int squareIndex = gl_VertexIndex % 12;
    outFrontFace = squareIndex < 6 ? 1.0 : -1.0;
    vec2 pixelCoord = ivec2(pixelIndex % mapSize.x, pixelIndex / mapSize.x) + squares[squareIndex];
    vec2 uvCoord = pixelCoord / vec2(mapSize);

	vec3 viewPosition = ScreenPosToViewPos(uvCoord, 0, lights[lightIndex].invProj);
	vec4 worldPosition = lights[lightIndex].invView * vec4(viewPosition, 1.0);

	float worldDepth;
	if (pixelCoord == clamp(pixelCoord, vec2(1), mapSize - 1)) {
		float depth = texture(shadowMap, uvCoord * lights[lightIndex].mapOffset.zw + lights[lightIndex].mapOffset.xy).r;
		worldDepth = depth * (lights[lightIndex].clip.y - lights[lightIndex].clip.x) + lights[lightIndex].clip.x;
	} else {
		worldDepth = lights[lightIndex].clip.x;
	}
	outWorldPos = lights[lightIndex].invView[3].xyz +
						normalize(worldPosition.xyz - lights[lightIndex].invView[3].xyz) * worldDepth;

	gl_Position = view.projMat * view.viewMat * vec4(outWorldPos, 1.0);
}
