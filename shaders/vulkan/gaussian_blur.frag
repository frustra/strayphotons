#version 450

#extension GL_OVR_multiview2 : enable
layout(num_views = 2) in;

// Separable Gaussian filter, primarily used for bloom

layout(binding = 0) uniform sampler2DArray sourceTex;

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outFragColor;

const int size = 15;
const int left = (size - 1) / 2;

// clang-format off
float kernel[size] = float[](
    //0.009033, 0.018476, 0.033851, 0.055555, 0.08167, 0.107545, 0.126854, 0.134032, 0.126854, 0.107545, 0.08167, 0.055555, 0.033851, 0.018476, 0.009033
    //0.000489, 0.002403, 0.009246, 0.02784, 0.065602, 0.120999, 0.174697, 0.197448, 0.174697, 0.120999, 0.065602, 0.02784, 0.009246, 0.002403, 0.000489
    0.000137, 0.000971, 0.005087, 0.019712, 0.056514, 0.119899, 0.188269, 0.218824, 0.188269, 0.119899, 0.056514, 0.019712, 0.005087, 0.000971, 0.000137
);
// clang-format on

layout(push_constant) uniform PushConstants {
    vec2 direction;
    float threshold;
    float scale;
};

void main() {
    vec3 sum = vec3(0.0);
    vec2 texsize = textureSize(sourceTex, 0).xy;

    for (int i = 0; i < size; i++) {
        vec2 offset = (float(i - left) * direction * 2.0 + 0.5) / texsize;
        vec3 px = min(texture(sourceTex, vec3(inTexCoord + offset, gl_ViewID_OVR)).rgb, vec3(threshold));
        sum += px * kernel[i];
    }

    outFragColor = vec4(sum * scale, 1.0);
}
