#version 450

layout(binding = 0) uniform sampler2D luminanceTex;
layout(binding = 1) uniform sampler2D blurTex1;
layout(binding = 2) uniform sampler2D blurTex2;

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outFragColor;

const float weight1 = 0.4;
const float weight2 = 0.5;

void main() {
	vec3 lum = texture(luminanceTex, inTexCoord).rgb;
	if (any(isnan(lum))) lum = vec3(0.0);

	vec3 bloom1 = texture(blurTex1, inTexCoord).rgb;
	vec3 bloom2 = texture(blurTex2, inTexCoord).rgb;

	outFragColor = vec4(lum + bloom1 * weight1 + bloom2 * weight2, 1.0);
}
