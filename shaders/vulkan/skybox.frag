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
// #include "../lib/perlin.glsl"

INCLUDE_LAYOUT(binding = 0)
#include "lib/view_states_uniform.glsl"

INCLUDE_LAYOUT(binding = 1)
#include "lib/exposure_state.glsl"

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform PushConstants {
    mat4 rotation;
    uint index;
    float brightness;
    float density;
    float starSize;
};

const vec3[3] colorTints = vec3[](vec3(255, 182, 119) / 255, vec3(255, 252, 246) / 255, vec3(188, 210, 255) / 255);
// const vec3[4] dustColors = vec3[](vec3(81, 30, 30) / 255, vec3(226, 158, 56) / 255, vec3(85, 131, 185) / 255, vec3(219, 216, 220) / 255);

void main() {
    ViewState view = views[gl_ViewID_OVR];

    vec2 flippedCoord = vec2(inTexCoord.x, 1 - inTexCoord.y);
    vec3 rayDir = normalize(
        mat3(rotation) * mat3(view.invViewMat) * ScreenPosToViewPos(flippedCoord, 1, view.invProjMat));
    rayDir = clamp(rayDir, -1, 1);

    float azimuth = atan(rayDir.z, rayDir.x) * (0.5 / M_PI) + 0.5;
    float elevation = asin(rayDir.y) * (1 / M_PI) + 0.5;

    vec2 cellSize = vec2(1.5, 1.0) * density;
    float distFromEquator = floor(abs(cellSize.y * 0.5 - elevation * cellSize.y)) / cellSize.y;
    float rowScale = ceil(cos(distFromEquator * M_PI) * cellSize.x);
    vec2 viewPos = vec2(azimuth * rowScale, elevation * cellSize.y);
    float viewportScale = 1.0 / sqrt(length(view.extents));

    ivec2 cellNum = ivec2(viewPos);

    vec4 rng = randState(cellNum.xyx);
    float pointSize = max(0.00004, rand2(rng) * starSize);
    vec2 offsetPos = vec2(rand2(rng), rand2(rng));
    float edgeScale = density * viewportScale * pointSize * 50;
    offsetPos = clamp(offsetPos, min(0.5, edgeScale), max(0.5, 1 - edgeScale));
    vec2 starAngle = (cellNum + offsetPos) / vec2(rowScale, cellSize.y);
    starAngle.x = (starAngle.x - 0.5) * M_PI * 2;
    starAngle.y = (starAngle.y - 0.5) * M_PI;
    vec3 starDir = vec3(cos(starAngle.x), sin(starAngle.y), sin(starAngle.x));
    starDir.xz *= cos(starAngle.y);

    vec3 color = colorTints[int(rand2(rng) * 3)].rgb;
    float pointDist = smoothstep(1 - pointSize * viewportScale, 1, dot(starDir, rayDir));
    float noise = rand2(rng) * rand2(rng) * rand2(rng) * rand2(rng);
    float randomBrightness = brightness * noise * noise * noise * noise;
    outFragColor = vec4(color * pointDist * randomBrightness * exposure, 1.0);
    // if (index == 0) {
    //     float baseNoise = PerlinNoise3D(rayDir * 4.2);
    //     float densityNoise = max(0, PerlinNoise3D(rayDir) + 0.2 + baseNoise * 0.1);
    //     densityNoise = densityNoise * densityNoise * 0.3;
    //     float noiseA = smoothstep(0.6, 0.8, baseNoise);
    //     float noiseB = smoothstep(0.2, 0.6, baseNoise) - noiseA;
    //     float noiseC = smoothstep(-0.5, 0.5, baseNoise) - noiseB - noiseA;
    //     // outFragColor.rgb += densityNoise * noiseA * noiseA * 20 * mix(dustColors[2], dustColors[3], noiseA);
    //     // outFragColor.rgb += densityNoise * noiseB * noiseB * 2 * mix(dustColors[1], dustColors[2], noiseB);
    //     // outFragColor.rgb += densityNoise * noiseC * noiseC * 0.5 * mix(dustColors[0], dustColors[1], noiseC);
    //     // outFragColor.rgb += densityNoise * noiseB * dustColors[1];
    //     // outFragColor.rgb += densityNoise * noiseC * dustColors[2];
    // }
    // outFragColor = vec4(vec3(fract(viewPos), 0) * exposure, 1.0) - vec4(outFragColor.rgb, 0);
}
