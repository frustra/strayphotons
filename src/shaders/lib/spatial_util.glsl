#ifndef SPATIAL_UTIL_GLSL_INCLUDED
#define SPATIAL_UTIL_GLSL_INCLUDED

// Inverse perspective divide to produce linear depth in (0, 1) relative to the
// camera and far plane.
float LinearDepth(float depth, vec2 clip) {
	depth = 2 * depth - 1;
	return 2 * clip.x / (clip.y + clip.x - depth * (clip.y - clip.x));
}

// Projects v onto a normalized vector u.
vec3 ProjectVec3(vec3 v, vec3 u) {
	return u * dot(v, u);
}

// Gradient noise in [-1, 1] as in [Jimenez 2014]
float InterleavedGradientNoise(vec2 position) {
	const vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
	return 2 * fract(magic.z * fract(dot(position, magic.xy))) - 1;
}

// Some nice sample coordinates around a spiral.
const vec2[8] SpiralOffsets = vec2[](
	vec2(-0.7071,  0.7071),
	vec2(-0.0000, -0.8750),
	vec2( 0.5303,  0.5303),
	vec2(-0.6250, -0.0000),
	vec2( 0.3536, -0.3536),
	vec2(-0.0000,  0.3750),
	vec2(-0.1768, -0.1768),
	vec2( 0.1250,  0.0000)
);

#endif
