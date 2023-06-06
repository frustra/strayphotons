/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#version 450

#include "../lib/util.glsl"

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outLinearDepth;

layout(binding = 0) uniform sampler2D shadowMap;

#include "../lib/shadow_sample.glsl"
#include "../lib/types_common.glsl"

layout(binding = 1) uniform PreviousLightData {
    Light previousLights[MAX_LIGHTS];
    int previousLightCount;
};

INCLUDE_LAYOUT(binding = 2)
#include "lib/light_data_uniform.glsl"

layout(push_constant) uniform PushConstants {
    uint lightIndex;
};

void main() {
    if (lights[lightIndex].parentIndex >= previousLightCount) discard;

    if (lights[lightIndex].previousIndex >= previousLightCount) {
        // This is the first frame this light exists, mask it off until it is included in the shadowmap
        outLinearDepth = vec4(0);
        return;
    }

    uint i = lights[lightIndex].previousIndex;
    uint j = lights[lightIndex].parentIndex;

    vec2 flippedCoord = vec2(inTexCoord.x, 1 - inTexCoord.y);
    vec3 fragPosition = ScreenPosToViewPos(flippedCoord, 0, previousLights[i].invProj);
    vec3 worldFragPosition = (previousLights[i].invView * vec4(fragPosition, 1.0)).xyz;

    // Sample the parent shadow map and check if we should write the mask
    vec3 shadowMapPos = (previousLights[j].view * vec4(worldFragPosition, 1.0)).xyz;
    ShadowInfo info = ShadowInfo(shadowMapPos,
        previousLights[j].proj,
        previousLights[j].mapOffset,
        previousLights[j].clip,
        previousLights[j].bounds);

    float occlusion = SimpleOcclusion(info);
    if (occlusion < 0.5) {
        outLinearDepth = vec4(0);
    } else {
        discard;
    }
}
