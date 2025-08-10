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

const vec3[3] colorTints = vec3[](vec3(255, 182, 119) / 255,
    vec3(255, 252, 246) / 255,
    vec3(188, 210, 255) / 255);

void main() {
    ViewState view = views[gl_ViewID_OVR];

    vec2 flippedCoord = vec2(inTexCoord.x, 1 - inTexCoord.y);
    vec3 rayDir = mat3(rotation) * normalize(mat3(view.invViewMat) * ScreenPosToViewPos(flippedCoord, 1, view.invProjMat));
    rayDir = clamp(rayDir, -1, 1);

    float latitude = atan(rayDir.z, rayDir.x) * (0.5 / M_PI) + 0.5;
    float longitude = asin(rayDir.y) * (1 / M_PI) + 0.5;

    vec2 cellSize = vec2(1.5, 1.0) * density;
    float distFromEquator = floor(abs(cellSize.y * 0.5 - longitude * cellSize.y)) / cellSize.y;
    float rowScale = ceil(cos(distFromEquator * M_PI) * cellSize.x);
    vec2 viewPos = vec2(latitude * rowScale, longitude * cellSize.y);

    ivec2 cellNum = ivec2(viewPos);

    vec4 rng = randState(cellNum.xyx);
    float pointSize = rand2(rng) * starSize;
    vec2 offsetPos = vec2(rand2(rng), rand2(rng));
    offsetPos *= clamp(1 - (pointSize * 100), 0, 1);
    offsetPos += clamp(pointSize * 50, 0, 0.5);
    vec2 starAngle = (cellNum + offsetPos) / vec2(rowScale, cellSize.y);
    starAngle.x = (starAngle.x - 0.5) * M_PI * 2;
    starAngle.y = (starAngle.y - 0.5) * M_PI;
    vec3 starDir = vec3(cos(starAngle.x), sin(starAngle.y), sin(starAngle.x));
    starDir.xz *= cos(starAngle.y);

    vec3 color = colorTints[int(rand2(rng)*3)].rgb * brightness * rand2(rng);
    outFragColor = vec4(color * smoothstep(1 - pointSize / sqrt(length(view.extents)), 1, dot(starDir, rayDir)) * exposure, 1.0);
    // outFragColor = vec4(vec3(fract(viewPos), 0) * exposure, 1.0);
}
