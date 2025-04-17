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
layout(binding = 5) uniform sampler3D voxelRadiance;
layout(binding = 6) uniform sampler3D voxelNormals;

layout(constant_id = 0) const int DEBUG_MODE = 0;
layout(constant_id = 1) const float BLEND_WEIGHT = 0;
layout(constant_id = 2) const int VOXEL_LAYER = 0;

#include "../lib/voxel_shared.glsl"

float GetVoxelNearest(vec3 position, int level, out vec3 radiance) {
    vec4 radianceData = texelFetch(voxelRadiance, ivec3(position) >> level, level);
    radiance = radianceData.rgb;
    return radianceData.a;
}

vec4 TraceGridLine(vec2 startPos, vec2 endPos, float height, int level) {
	vec2 delta = endPos - startPos;
	vec2 absDelta = abs(delta);
	ivec2 pos = ivec2(floor(startPos));
	vec2 dt = vec2(1.0) / absDelta;
	float t = 0;

	int n = 1;
	ivec2 inc = ivec2(sign(delta));
	vec2 tNext;

	if (delta.x == 0) {
		tNext.x = dt.x; // infinity
	} else if (delta.x > 0) {
		n += int(floor(endPos.x)) - pos.x;
		tNext.x = (floor(startPos.x) + 1 - startPos.x) * dt.x;
	} else {
		n += pos.x - int(floor(endPos.x));
		tNext.x = (startPos.x - floor(startPos.x)) * dt.x;
	}

	if (delta.y == 0) {
		tNext.y = dt.y; // infinity
	} else if (delta.y > 0) {
		n += int(floor(endPos.y)) - pos.y;
		tNext.y = (floor(startPos.y) + 1 - startPos.y) * dt.y;
	} else {
		n += pos.y - int(floor(endPos.y));
		tNext.y = (startPos.y - floor(startPos.y)) * dt.y;
	}

	for (; n > 0; n--) {
        vec3 radiance;
        float alpha = GetVoxelNearest(vec3(pos.x, height, pos.y), level, radiance);
		if (alpha > 0) {
            return vec4(radiance, alpha);
        }

		if (tNext.y < tNext.x) {
			pos.y += inc.y;
			t = tNext.y;
			tNext.y += dt.y;
		} else {
			pos.x += inc.x;
			t = tNext.x;
			tNext.x += dt.x;
		}
	}
	return vec4(0);
}

vec4 TraceIntervalCircle(vec3 samplePos, vec2 range, int samples, int level) {
	vec4 sum = vec4(0);
    float rOffset = 0;//M_PI / samples;// + InterleavedGradientNoise(gl_FragCoord.xy) * 2 * M_PI;

	float diffuseScale = 1.0 / samples;
	for (float r = 0.0; r < samples; r++) {
		float theta = r * 2 * M_PI * diffuseScale + rOffset;
        vec2 sampleDir = vec2(sin(theta), cos(theta));
		vec4 sampleColor = TraceGridLine(samplePos.xz + sampleDir * range.x, samplePos.xz + sampleDir * range.y, samplePos.y, level);
		sum += sampleColor * diffuseScale;
	}

    return sum;
}

float zoom = 2;

void main() {
    ViewState view = views[gl_ViewID_OVR];
    vec3 viewPos = view.invViewMat[3].xyz;
	viewPos.y -= 1.3;
    vec3 viewVoxelPos = (voxelInfo.worldToVoxel * vec4(viewPos, 1.0)).xyz;

	vec2 screenSize = textureSize(overlayTex, 0).xy;
	float visibleGridSize = max(voxelInfo.gridSize.x, voxelInfo.gridSize.z);
    vec2 scaledCoord = (inTexCoord - 0.5) * 0.5;
	scaledCoord.x *= (screenSize.x / screenSize.y);
	scaledCoord *= visibleGridSize / zoom;
    vec3 rayPos = viewVoxelPos;
	rayPos.xz += scaledCoord.xy;

    vec3 sampleRadiance;
    if (DEBUG_MODE == 1) {
        float alpha = GetVoxelNearest(rayPos, VOXEL_LAYER, sampleRadiance);
		sampleRadiance = max(sampleRadiance, vec3(alpha * 0.001));
    } else if (DEBUG_MODE == 2) {
        float alpha = GetVoxelNearest(rayPos, VOXEL_LAYER, sampleRadiance);
		vec4 sampleValue = TraceIntervalCircle(rayPos, vec2(0, 4), 8, VOXEL_LAYER);
		sampleRadiance = max(sampleValue.rgb, vec3(alpha * 0.001));
	} else if (DEBUG_MODE == 3) {
        float alpha = GetVoxelNearest(rayPos, VOXEL_LAYER, sampleRadiance);
		vec4 sampleValue = TraceIntervalCircle(rayPos, vec2(0, 4), 8, VOXEL_LAYER);
		sampleValue += TraceIntervalCircle(rayPos, vec2(4, 8), 16, VOXEL_LAYER);
		sampleRadiance = max(sampleValue.rgb, vec3(alpha * 0.001));
	}

    vec3 overlay = texture(overlayTex, vec3(inTexCoord, gl_ViewID_OVR)).rgb; // pre-exposed
    outFragColor = vec4(mix(sampleRadiance * exposure, overlay, BLEND_WEIGHT), 1.0);
}
