#version 430

##import lib/util
##import lib/lighting_util

layout (binding = 0) uniform sampler2D luminanceTex;

layout (location = 0) in vec2 inTexCoord;
layout (location = 0) out vec4 outFragColor;

const float scale = 0.1;

void main() {
	vec3 lum = texture(luminanceTex, inTexCoord).rgb; // pre-exposed
	if (any(isnan(lum))) {
		outFragColor = vec4(vec3(0), 1);
	} else {
		outFragColor = vec4(lum * scale, 1);
	}
}
