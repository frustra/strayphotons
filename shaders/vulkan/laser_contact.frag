#version 450

#extension GL_OVR_multiview2 : enable
layout(num_views = 2) in;

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in vec3 inRadiance;
layout(location = 2) in float inScale;
layout(location = 0) out vec4 outFragColor;

void main() {
    float dist = length(abs(inTexCoord - 0.5));
    float weight = (1 - smoothstep(0.1, 0.48, dist)) * inScale;

    outFragColor.rgb = inRadiance;
    outFragColor.a = max(weight, 0);
}
