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

layout(set = 1, binding = 0) uniform sampler2DArray radianceProbes[];
layout(set = 2, binding = 0) uniform sampler2DArray radianceCascade[];

layout(constant_id = 0) const int DEBUG_MODE = 0;
layout(constant_id = 1) const float BLEND_WEIGHT = 0;
layout(constant_id = 2) const float ZOOM = 1;
layout(constant_id = 3) const int BASE_SAMPLES = 4;
layout(constant_id = 4) const int NEXT_SAMPLES = 4;
layout(constant_id = 5) const float SAMPLE_LENGTH = 8;

#include "lib/rc_shared.glsl"

vec4 TraceCircle(vec3 samplePos) {
    float rOffset = 0;//M_PI / samples;// + InterleavedGradientNoise(gl_FragCoord.xy) * 2 * M_PI;

	float voxelScale = float(textureSize(radianceProbes[0], 0).x) / voxelInfo.gridSize.x;

	int c0 = BASE_SAMPLES;
	float c0_len = SAMPLE_LENGTH;
	int c1 = c0 * NEXT_SAMPLES;
	float c1_len = SAMPLE_LENGTH * NEXT_SAMPLES;
	float invC0 = 1.0 / c0;
	float invC1 = 1.0 / c1;

	vec4 samples[BASE_SAMPLES];
	for (int r0 = 0; r0 < c0; r0++) {
		float theta0 = r0 * 2 * M_PI * invC0;
        vec2 sampleDir0 = vec2(sin(theta0 + rOffset), cos(theta0 + rOffset));
		// samples[r0] = TraceGridLine(samplePos.xz, samplePos.xz + sampleDir0 * c0_len, samplePos.y, 0);
		samples[r0] = texelFetch(radianceProbes[0], ivec3(samplePos.xz * voxelScale, r0), 0);
		if (samples[r0].a > 0) continue;

		for (int n1 = -NEXT_SAMPLES / 2; n1 < int(ceil(NEXT_SAMPLES / 2.0)); n1++) {
			int r1 = r0 * NEXT_SAMPLES + n1;
			if (r1 < 0) r1 += c1;
			float theta1 = (r1 + fract((NEXT_SAMPLES - 1.0) / 2.0)) * 2 * M_PI * invC1;
		    vec2 sampleDir1 = vec2(sin(theta1 + rOffset), cos(theta1 + rOffset));
			vec2 startPos = samplePos.xz + sampleDir0 * c0_len;
			vec2 endPos = samplePos.xz + sampleDir1 * (c0_len + c1_len);
			vec2 sampleDir = normalize(endPos - startPos);
			float theta = atan(sampleDir.x, sampleDir.y);
			if (theta < 0) theta += 2.0 * M_PI;
			int r = int(ceil(theta / 2.0 / M_PI * c1)) % c1;
			// float theta2 = r * 2 * M_PI * invC1;
        	// vec2 sampleDir2 = vec2(sin(theta2 + rOffset), cos(theta2 + rOffset));
			vec4 sampleC1 = TraceGridLine(startPos, startPos + sampleDir * c1_len, samplePos.y, 0);
			ivec3 samplePos2 = XYRtoUVW(ivec3(startPos * voxelScale, r), c1 / textureSize(radianceProbes[1], 0).z, 1);
			// vec4 sampleC1 = texelFetch(radianceProbes[1], samplePos2, 0);
			samples[r0] += sampleC1 * invC1;
		}
	}

	vec4 sum = vec4(0);
	for (int r = 0; r < c0; r++) {
		sum += samples[r] * invC0;
	}
    return sum;
}

vec4 TraceCircle2(vec3 samplePos) {
    float rOffset = 0;//M_PI / samples;// + InterleavedGradientNoise(gl_FragCoord.xy) * 2 * M_PI;

	vec2 voxelScale = float(textureSize(radianceCascade[0], 0).xy) / voxelInfo.gridSize.xz;

	int c0 = BASE_SAMPLES;
	float invC0 = 1.0 / c0;

	vec4 sum = vec4(0);
	for (int r0 = 0; r0 < c0; r0++) {
		sum += texelFetch(radianceCascade[0], ivec3(samplePos.xz * voxelScale, r0), 0) * invC0;
	}
    return sum;
}

void main() {
    ViewState view = views[gl_ViewID_OVR];
    vec3 viewPos = view.invViewMat[3].xyz;
    vec3 viewVoxelPos = (voxelInfo.worldToVoxel * vec4(viewPos, 1.0)).xyz;
	viewPos.y -= voxelProjectOffset;
    vec3 feetVoxelPos = (voxelInfo.worldToVoxel * vec4(viewPos, 1.0)).xyz;

	vec2 screenSize = textureSize(overlayTex, 0).xy;
	float visibleGridSize = max(voxelInfo.gridSize.x, voxelInfo.gridSize.z);
    vec2 scaledCoord = (inTexCoord - 0.5) * 0.5;
	scaledCoord.x *= (screenSize.x / screenSize.y);
	scaledCoord *= visibleGridSize / ZOOM;
    vec3 rayPos = feetVoxelPos;
	rayPos.xz += scaledCoord.xy;

    vec3 sampleRadiance;
    if (DEBUG_MODE == 1) {
        float alpha = GetVoxelNearest(rayPos, 0, sampleRadiance);
		sampleRadiance = max(sampleRadiance, vec3(alpha * 0.001));
    } else if (DEBUG_MODE == 2) {
        float alpha = GetVoxelNearest(rayPos, 0, sampleRadiance);
		vec4 sampleValue = TraceIntervalCircle(rayPos, vec2(0, 4), 8, 0);
		sampleRadiance = max(sampleValue.rgb, vec3(alpha * 0.001));
	} else if (DEBUG_MODE == 3) {
        float alpha = GetVoxelNearest(rayPos, 0, sampleRadiance);
		vec4 sampleValue = TraceIntervalCircle(rayPos, vec2(0, 4), 8, 0);
		sampleValue += TraceIntervalCircle(rayPos, vec2(4, 8), 16, 0);
		sampleRadiance = max(sampleValue.rgb, vec3(alpha * 0.001));
	} else if (DEBUG_MODE == 4) {
        float alpha = GetVoxelNearest(rayPos, 0, sampleRadiance);
		vec4 sampleValue = TraceCircle(rayPos);
		sampleRadiance = max(sampleValue.rgb, vec3(alpha * 0.001));
	} else if (DEBUG_MODE == 5) {
        float alpha = GetVoxelNearest(rayPos, 0, sampleRadiance);
		vec4 sampleValue = TraceCircle2(rayPos);
		sampleRadiance = max(sampleValue.rgb, vec3(alpha * 0.001));
	} else if (DEBUG_MODE == 6) {
		vec2 flippedCoord = vec2(inTexCoord.x, 1 - inTexCoord.y);
		vec4 rayPos2 = view.invViewMat * vec4(ScreenPosToViewPos(flippedCoord, 0, view.invProjMat), 1);
		vec3 rayDir = normalize(rayPos2.xyz - view.invViewMat[3].xyz);
		float t = (-viewVoxelPos.y) / rayDir.y;
		if (rayDir.y < -0.00001 && t > 0) {
			rayPos = viewVoxelPos + rayDir * t;
			float alpha = GetVoxelNearest(rayPos, 0, sampleRadiance);
			vec4 sampleValue = TraceCircle2(rayPos);
			sampleRadiance = max(sampleValue.rgb, vec3(alpha * 0.001));
		} else {
			sampleRadiance = vec3(0);
		}
	}

    vec3 overlay = texture(overlayTex, vec3(inTexCoord, gl_ViewID_OVR)).rgb; // pre-exposed
    outFragColor = vec4(mix(sampleRadiance * exposure, overlay, BLEND_WEIGHT), 1.0);
}
