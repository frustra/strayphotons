/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "rc_util.glsl"

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
