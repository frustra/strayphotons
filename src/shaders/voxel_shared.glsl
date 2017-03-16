#ifndef VOXEL_SHARED_GLSL_INCLUDED
#define VOXEL_SHARED_GLSL_INCLUDED

##import lib/util

const uint MipmapWorkGroupSize = 256;
const float VoxelFixedPointExposure = 64.0;

const uint[13] MaxFragListMask = uint[](8191, 4095, 2047, 1023, 511, 255, 127, 63, 31, 15, 7, 3, 1);
const uint[13] FragListWidthBits = uint[](13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1);

const mat4[3] AxisSwapForward = mat4[](
	mat4(mat3(0, 0, -1, 0, 1, 0, 1, 0, 0)),
	mat4(mat3(1, 0, 0, 0, 0, -1, 0, 1, 0)),
	mat4(1.0)
);

const mat3[3] AxisSwapReverse = mat3[](
	mat3(0, 0, 1, 0, 1, 0, -1, 0, 0),
	mat3(1, 0, 0, 0, 0, 1, 0, -1, 0),
	mat3(1.0)
);

const vec3[6] AxisVector = vec3[](
	vec3(1, 0, 0),
	vec3(0, 1, 0),
	vec3(0, 0, 1),
	vec3(-1, 0, 0),
	vec3(0, -1, 0),
	vec3(0, 0, -1)
);

int SignedDominantAxis(vec3 normal)
{
	vec3 absNormal = abs(normal);
	bvec3 mask = greaterThanEqual(absNormal.xyz, max(absNormal.yzx, absNormal.zxy));
	return int(dot(vec3(mask) * sign(normal), vec3(1, 2, 3)));
}

int DominantAxis(vec3 normal)
{
	int axis = SignedDominantAxis(normal);
	return abs(axis) - 1 + int(step(0, -axis)) * 3;
}

#endif
