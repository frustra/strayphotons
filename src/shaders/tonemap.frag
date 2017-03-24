#version 430

// #define DEBUG_OVEREXPOSED
// #define ENABLE_DITHER

##import lib/util
##import lib/lighting_util

layout (binding = 0) uniform sampler2D luminanceTex;

layout (location = 0) in vec2 inTexCoord;
layout (location = 0) out vec4 outFragColor;

const float ditherAmount = 0.5 / 255.0;
uniform vec2 saturation = vec2(0, 1);

void main() {
	vec4 luminosity = texture(luminanceTex, inTexCoord); // pre-exposed

	vec3 toneMapped = HDRTonemap(luminosity.rgb * 2.6) / HDRTonemap(vec3(8.0));

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

	vec3 hsv = RGBtoHSV(toneMapped);
	hsv.y = mix(saturation.x, saturation.y, hsv.y);
	outFragColor = vec4(HSVtoRGB(hsv), luminosity.a);
}
