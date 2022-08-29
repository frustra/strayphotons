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

void main() {
    vec2 flippedCoord = vec2(inTexCoord.x, 1 - inTexCoord.y);

    // Find the light corresponding to this spot on the shadowmap
    for (int i = 0; i < lightCount; i++) {
        if (all(greaterThanEqual(flippedCoord, lights[i].mapOffset.xy)) &&
            all(lessThan(flippedCoord, lights[i].mapOffset.xy + lights[i].mapOffset.zw))) {

            if (lights[i].previousIndex >= previousLightCount) {
                // This is the first frame this light exists, mask it off until it is included in the shadowmap
                outLinearDepth = vec4(0);
                return;
            }

            if (lights[i].parentIndex >= previousLightCount) break;
            uint j = lights[i].previousIndex;
            uint k = lights[i].parentIndex;

            vec2 texCoord = (flippedCoord - lights[i].mapOffset.xy) / lights[i].mapOffset.zw;
            vec3 fragPosition = ScreenPosToViewPos(texCoord, 0, previousLights[j].invProj);
            vec3 worldFragPosition = (previousLights[j].invView * vec4(fragPosition, 1.0)).xyz;

            // Sample the parent shadow map and check if we should write the mask
            vec3 shadowMapPos = (previousLights[k].view * vec4(worldFragPosition, 1.0)).xyz;
            ShadowInfo info = ShadowInfo(shadowMapPos,
                previousLights[k].proj,
                previousLights[k].mapOffset,
                previousLights[k].clip,
                previousLights[k].bounds);

            float occlusion = SimpleOcclusion(info);
            if (occlusion < 0.5) {
                outLinearDepth = vec4(0);
                return;
            }
        }
    }

    discard;
}
