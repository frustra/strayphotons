#version 430

##import lib/util
##import lib/lighting_util

layout (binding = 0) uniform sampler2D luminanceTex;

layout (location = 0) in vec2 inTexCoord;
layout (location = 0) out vec4 outFragColor;

const float threshold = 1.0;

void main() {
	vec3 lum = texture(luminanceTex, inTexCoord).rgb; // pre-exposed

	float brightness = max(DigitalLuminance(lum) - threshold, 0.0);
	brightness /= (1.0 + brightness);
	
	outFragColor = vec4(brightness * lum, 1.0);
}
