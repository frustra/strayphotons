#version 430

uniform float threshold = 0.7;

layout (binding = 0) uniform sampler2D luminanceTex;

layout (location = 0) in vec2 inTexCoord;
layout (location = 0) out vec4 outFragColor;

float DigitalLuminance(vec3 linear) {
	// ITU-R Recommendation BT. 601

	return dot(linear, vec3(0.299, 0.587, 0.114));
}


void main() {
	vec3 lum = texture(luminanceTex, inTexCoord).rgb;

	float brightness = max(DigitalLuminance(lum) - threshold, 0.0);
	brightness /= (1.0 + brightness);
	
	outFragColor = vec4(brightness * lum, 1.0);
}
