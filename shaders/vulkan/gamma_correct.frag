#version 450

#extension GL_OVR_multiview2 : enable
layout(num_views = 2) in;

#include "../lib/util.glsl"

layout(binding = 0) uniform sampler2DArray tex;
layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outFragColor;

void main() {
    vec4 frag = texture(tex, vec3(inTexCoord, gl_ViewID_OVR));
    outFragColor = vec4(FastLinearToSRGB(frag.rgb), frag.a);
}
