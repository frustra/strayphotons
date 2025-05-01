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

vec4 TraceGridLineScaled(vec2 startPosIn, vec2 endPosIn, float height, float scale) {
	vec2 startPos = startPosIn / scale;
	vec2 endPos = endPosIn / scale;
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
        // vec4 sampleValue = texture(voxelRadiance, vec3(pos.x + 0.5, height + 0.5, pos.y + 0.5) / (textureSize(voxelRadiance, 0)));
		// if (sampleValue.a > 0) {
        //     return sampleValue;
        // }
        vec3 radiance;
        float alpha = GetVoxelNearest(vec3(pos.x, height, pos.y), 0, radiance);
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

float LineCircleIntersection(vec2 lineStart, vec2 lineEnd, vec2 circleCenter, float circleRadius) {
	float dist = length(lineEnd - lineStart);
	vec2 dir = normalize(lineEnd - lineStart);
    vec2 tangent = vec2(dir.y, -dir.x);
    float distanceToLine = dot(tangent, lineStart - circleCenter);
	float distanceAlongLine = dot(dir, circleCenter - lineStart);

	if (abs(distanceToLine) <= circleRadius && distanceAlongLine >= 0 && distanceAlongLine < dist) {
		return distanceAlongLine;
	} else {
		return 1.0/0.0;
	}
}

float LinePlaneIntersection(vec2 lineStart, vec2 lineEnd, vec2 planeOrigin, vec2 planeNormal) {
	vec2 normal = normalize(planeNormal);
	float distStart = dot(normal, lineStart - planeOrigin);
	float distEnd = dot(normal, lineEnd - planeOrigin);
	if (distStart < 0) {
		return distStart;
	} else if (distEnd < 0) {
		return length(lineEnd - lineStart) * (distStart / (distStart - distEnd));
	} else {
		return 1.0/0.0;
	}
}

float LineBoxIntersection(vec2 lineStart, vec2 lineEnd, vec2 boxOrigin, vec2 boxNormal) {
	vec2 tanget = vec2(boxNormal.y, -boxNormal.x);
	mat2 transform = inverse(mat2(boxNormal, tanget));
	vec2 rayStart = transform * (lineStart - boxOrigin);
	vec2 rayEnd = transform * (lineEnd - boxOrigin);
	vec2 rayDir = normalize(rayEnd - rayStart);
    vec2 tMin = (vec2(-1) - rayStart) / rayDir;
    vec2 tMax = (vec2(1) - rayStart) / rayDir;
    vec2 t1 = min(tMin, tMax);
    vec2 t2 = max(tMin, tMax);
    float tNear = max(t1.x, t1.y);
    float tFar = min(t2.x, t2.y);
	if (tFar > 0 && tNear < 0) {
		return tNear;
	} else if (tFar > 0 && tNear < tFar && tNear < length(rayEnd - rayStart)) {
		return length(mat2(boxNormal, tanget) * (rayDir * tNear));
	} else {
		return 1.0/0.0;
	}
}

#define DEPTH_ADD(name, color, expr)                        \
float name = (expr);                                        \
if (!isinf(name) && (isinf(result.a) || result.a > name)) { \
	result = vec4(color, name);                             \
}															\
if (result.a <= 0) return vec4(clamp(result.rgb, 0, 1), 1);

vec4 TraceSceneLine(vec2 startPos, vec2 endPos, vec2 circleCenter, float sceneScale) {
	vec4 result = vec4(vec3(0), 1.0/0.0);
	DEPTH_ADD(tallBox, vec3(0), LineBoxIntersection(startPos, endPos, vec2(0.35, 0.37), vec2(0.04, 0.13)))
	DEPTH_ADD(shortBox, vec3(0), LineBoxIntersection(startPos, endPos, vec2(0.65, 0.66), vec2(0.13, 0.04)))
	DEPTH_ADD(topWall, vec3(0.1), LinePlaneIntersection(startPos, endPos, vec2(0.051), vec2(0, 1)))
	DEPTH_ADD(rightWall, vec3(0, 0, 0.1), LinePlaneIntersection(startPos, endPos, vec2(0.933), vec2(-1, 0)))
	DEPTH_ADD(bottomWall, vec3(0, 0, 0), LinePlaneIntersection(startPos, endPos, vec2(0.921), vec2(0, -1)))
	DEPTH_ADD(leftWall, vec3(0.1, 0, 0), LinePlaneIntersection(startPos, endPos, vec2(0.067), vec2(1, 0)))
	// DEPTH_ADD(circle, vec3(1), LineCircleIntersection(startPos, endPos, circleCenter, sceneScale))
	// DEPTH_ADD(circle, vec3(0.5), LineBoxIntersection(startPos, endPos, circleCenter, vec2(sceneScale)))
	if (isinf(result.a)) return vec4(0);
	return clamp(result, 0, 1);
}
