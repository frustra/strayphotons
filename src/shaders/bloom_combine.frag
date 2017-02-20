#version 430

layout (binding = 0) uniform sampler2D luminanceTex;
layout (binding = 1) uniform sampler2D highpassTex1;
layout (binding = 2) uniform sampler2D highpassTex2;

layout (location = 0) in vec2 inTexCoord;
layout (location = 0) out vec4 outFragColor;

uniform float weight1 = 0.5;
uniform float weight2 = 0.5;

void main() {
	vec3 lum = texture(luminanceTex, inTexCoord).rgb;
	vec3 bloom1 = texture(highpassTex1, inTexCoord).rgb;
	vec3 bloom2 = texture(highpassTex2, inTexCoord).rgb;

	outFragColor = vec4(lum + bloom1 * weight1 + bloom2 * weight2, 1.0);
}