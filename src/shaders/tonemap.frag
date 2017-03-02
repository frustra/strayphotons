#version 430

// #define DEBUG_OVEREXPOSED
// #define ENABLE_DITHER

##import lib/util
##import lib/lighting_util

layout (binding = 0) uniform sampler2D luminanceTex;

layout (location = 0) in vec2 inTexCoord;
layout (location = 0) out vec4 outFragColor;

const float ditherAmount = 0.5 / 255.0;

void main() {
	vec4 luminosity = texture(luminanceTex, inTexCoord); // pre-exposed

	vec3 toneMapped = HDRTonemap(luminosity.rgb) / HDRTonemap(vec3(1.0));

#ifdef DEBUG_OVEREXPOSED
	if (toneMapped.r > 1 || toneMapped.g > 1 || toneMapped.b > 1) {
		// Highlight overexposed/blown out areas.
		toneMapped = vec3(1, 0, 0);
	}
#endif

#ifdef ENABLE_DITHER
	vec4 rng = randState(inTexCoord.xyx);
	toneMapped += (rand2(rng) - 0.5) * ditherAmount;
#endif

	outFragColor = vec4(toneMapped, luminosity.a);
}
