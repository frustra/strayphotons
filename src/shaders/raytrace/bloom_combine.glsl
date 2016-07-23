#version 430

layout (binding = 0) uniform sampler2D luminanceTex;
layout (binding = 1) uniform sampler2D highpassTex;

layout (location = 0) in vec2 inTexCoord;
layout (location = 0) out vec4 outFragColor;

void main() {
	vec3 lum = texture(luminanceTex, inTexCoord).rgb;
	vec3 bloom = texture(highpassTex, inTexCoord).rgb;

	outFragColor = vec4(lum + bloom, 1.0);
}
