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

// Returns a ray from the camera to the far plane.
vec3 FarPlaneRay(vec2 viewSpacePosition, float tanHalfFov, float aspectRatio) {
	return vec3(
		viewSpacePosition.x * tanHalfFov * aspectRatio,
		viewSpacePosition.y * tanHalfFov,
		1.0
	);
}

// Returns the view-space position of clip-space position.
vec3 ClipPosToViewPos(vec3 clipPos, mat4 invProj) {
	vec4 hpos = invProj * vec4(clipPos, 1.0); // Homogeneous position in view-space.
	return hpos.xyz / hpos.w; // Inverse perspective divide.
}

// Returns the view-space position of a screen-space texcoord and depth.
vec3 ScreenPosToViewPos(vec2 texCoord, float depth, mat4 invProj) {
	vec3 clip = vec3(texCoord, depth) * 2.0 - 1.0;
	return ClipPosToViewPos(clip, invProj);
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
