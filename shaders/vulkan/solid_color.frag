#version 450

#extension GL_OVR_multiview2 : enable
layout(num_views = 2) in;

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform PushConstants {
    vec4 color;
};

void main() {
    outFragColor = color;
}
