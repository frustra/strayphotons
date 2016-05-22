#ifndef COLOR_UTIL_GLSL_INCLUDED
#define COLOR_UTIL_GLSL_INCLUDED

// Maps a single channel of linear color to sRGB space.
float LinearToSRGBChannel(float linear) {
	if (linear < 0.00313067) return linear * 12.92;
	return pow(linear, (1.0/2.4)) * 1.055 - 0.055;
}

// Maps a linear RGB color to sRGB space.
vec3 LinearToSRGB(vec3 linear) {
	return vec3(
		LinearToSRGBChannel(linear.r),
		LinearToSRGBChannel(linear.g),
		LinearToSRGBChannel(linear.b)
	);
}

#endif
