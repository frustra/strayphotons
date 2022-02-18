#version 450

layout(binding = 0) uniform sampler2D tex;
layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outFragColor;

// 2 bits per channel
// 0bAABBGGRR
// RR=0, GG=1, BB=2, AA=3 => rgba = rgba
layout(constant_id = 0) const uint SWIZZLE = 0xE4;

void main() {
    vec4 value = texture(tex, inTexCoord);
    for (uint i = 0; i < 4; i++) {
        outFragColor[i] = value[(SWIZZLE & (3 << (2 * i))) >> (2 * i)];
    }
}
