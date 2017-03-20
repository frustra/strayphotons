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

int DominantAxis(vec3 normal)
{
	vec3 absNormal = abs(normal);
	bvec3 mask = greaterThanEqual(absNormal.xyz, max(absNormal.yzx, absNormal.zxy));
	return int(dot(vec3(mask) * sign(normal), vec3(1, 2, 3)));
}

#ifdef INTEL_GPU
#define UIMAGE3D uimage3D
#else
#define UIMAGE3D layout(r32ui) uimage3D
#endif

float ReadVoxelAndClear(UIMAGE3D packedVoxelImg, ivec3 position, out vec3 outRadiance, out vec3 outNormal)
{
	ivec3 index = position * ivec3(3, 2, 1);

	uint data = imageAtomicExchange(packedVoxelImg, index, uint(0));
	float radianceRed = float(data) / 0xFFFF;

	data = imageAtomicExchange(packedVoxelImg, index + ivec3(1, 0, 0), uint(0));
	float radianceGreen = float(data) / 0xFFFF;

	data = imageAtomicExchange(packedVoxelImg, index + ivec3(2, 0, 0), uint(0));
	float radianceBlue = float(data) / 0xFFFF;

	data = imageAtomicExchange(packedVoxelImg, index + ivec3(0, 1, 0), uint(0));
	float normalX = float(data) / 0xFFFF;

	data = imageAtomicExchange(packedVoxelImg, index + ivec3(1, 1, 0), uint(0));
	float normalY = float(data) / 0xFFFF;

	data = imageAtomicExchange(packedVoxelImg, index + ivec3(2, 1, 0), uint(0));
	float normalZ = float((data & 0xFFFFFF00) >> 8) / 0xFFFF;

	float count = float(data & 0xFF);
	outRadiance = vec3(radianceRed, radianceGreen, radianceBlue) / count;
	outNormal = vec3(normalX, normalY, normalZ) / count;
	return count;
}

#endif
