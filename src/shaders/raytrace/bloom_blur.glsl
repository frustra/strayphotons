#version 430

uniform vec2 direction;

layout (binding = 0) uniform sampler2D highpassTex;

layout (location = 0) in vec2 inTexCoord;
layout (location = 0) out vec4 outFragColor;

//const int size = 15;
//const int left = (size - 1) / 2;

//float kernel[size] = float[](
//	0.009033, 0.018476, 0.033851, 0.055555, 0.08167, 0.107545, 0.126854, 0.134032, 0.126854, 0.107545, 0.08167, 0.055555, 0.033851, 0.018476, 0.009033
//);

const int size = 21;
const int left = (size - 1) / 2;

float kernel[size] = float[](
	0.004481, 0.008089, 0.013722, 0.021874, 0.032768, 0.046128, 0.061021, 0.075856, 0.088613, 0.097274, 0.100346, 0.097274, 0.088613, 0.075856, 0.061021, 0.046128, 0.032768, 0.021874, 0.013722, 0.008089, 0.004481
);

void main() {
	vec3 accum = vec3(0.0);
	vec2 texsize = textureSize(highpassTex, 0);

	for (int i = 0; i < size; i++) {
		vec2 offset = float(i - left) * direction / texsize;
		vec3 px = texture(highpassTex, inTexCoord + offset).rgb;
		if (any(isnan(px))) continue;
		accum += px * kernel[i];
	}

	outFragColor = vec4(accum, 1.0);
}
