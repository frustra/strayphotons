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

// Maps a linear RGB color to sRGB space.
// http://chilliant.blogspot.ca/2012/08/srgb-approximations-for-hlsl.html
vec3 FastLinearToSRGB(vec3 linear) {
	vec3 S1 = sqrt(linear);
	vec3 S2 = sqrt(S1);
	vec3 S3 = sqrt(S2);
	return 0.662002687 * S1 + 0.684122060 * S2 - 0.323583601 * S3 - 0.0225411470 * linear;
}

// Maps an sRGB color to linear space.
// http://chilliant.blogspot.ca/2012/08/srgb-approximations-for-hlsl.html
vec3 FastSRGBToLinear(vec3 srgb) {
	return srgb * (srgb * (srgb * 0.305306011 + 0.682171111) + 0.012522878);
}

#endif
