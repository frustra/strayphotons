#version 450

#extension GL_OVR_multiview2 : enable
layout(num_views = 2) in;

#include "../lib/media_density.glsl"
#include "../lib/util.glsl"

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in vec3 inWorldPos;
layout(location = 2) in float inScale;
layout(location = 3) in vec3 inContactRadiance;
layout(location = 4) in float dUVbydFragCoord;
layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform PushConstants {
    vec3 radiance;
    float radius;
    vec3 start;
    float mediaDensityFactor;
    vec3 end;
    float time;
};

void main() {
    float dist = abs(inTexCoord.x - 0.5);

    float weight = (1 - smoothstep(0.1, 0.48, dist)) * inScale;

    if (mediaDensityFactor > 0) {
        float density = MediaDensity(inWorldPos, time);
        density = mediaDensityFactor * (density - 1) + 1;
        weight *= pow(density, 5);
    }

    outFragColor.rgb = radiance;
    outFragColor.a = max(weight, 0);
}
