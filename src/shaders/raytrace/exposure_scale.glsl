#version 430

uniform float exposure = 1.0;

layout (binding = 0) uniform sampler2D luminanceTex;

layout (location = 0) in vec2 inTexCoord;
layout (location = 0) out vec4 outFragColor;

void main() {
	vec3 lum = texture(luminanceTex, inTexCoord).rgb;

	outFragColor = vec4(exposure * lum, 1.0);
}
