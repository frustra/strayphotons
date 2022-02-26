#version 450

#extension GL_OVR_multiview2 : enable
layout(num_views = 2) in;

#include "../lib/util.glsl"

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in vec3 inWorldPos;
layout(location = 2) in float inScale;
layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform PushConstants {
    vec3 color;
    float radius;
    vec3 start;
    vec3 end;
    float time;
}
constants;

void main() {
    float dist = abs(inTexCoord.x - 0.5);

    float weight = (1 - smoothstep(0.3, 0.48, dist));
    weight *= (PerlinNoise(inWorldPos.xz * 3 + constants.time * 0.4) * 2 + 1);
    weight *= (PerlinNoise(inWorldPos.zx * 10 - constants.time * 1.4) * 1 + 0.5);

    outFragColor.rgb = constants.color;
    outFragColor.a = max(weight * inScale, 0);
}
