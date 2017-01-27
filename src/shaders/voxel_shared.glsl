#ifndef VOXEL_SHARED_GLSL_INCLUDED
#define VOXEL_SHARED_GLSL_INCLUDED

##import lib/util

const uint MipmapWorkGroupSize = 256;

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

#ifdef INTEL_GPU
#define UIMAGE3D uimage3D
#else
#define UIMAGE3D layout(r32ui) uimage3D
#endif

float ReadVoxelAndClear(UIMAGE3D packedVoxelImg, ivec3 position, out vec3 outColor, out vec3 outNormal, out vec3 outRadiance, out float outRoughness)
{
	ivec3 index = position * ivec3(6, 1, 1);

	uint data = imageAtomicExchange(packedVoxelImg, index, uint(0)).r;
	float colorRed = float((data & 0xFFFF0000) >> 16) / 0xFF;
	float colorGreen = float(data & 0xFFFF) / 0xFF;

	data = imageAtomicExchange(packedVoxelImg, index + ivec3(1, 0, 0), uint(0)).r;

	float colorBlue = float((data & 0xFFFF0000) >> 16) / 0xFF;
	float radianceRed = float(data & 0xFFFF) / 0x7FF;

	data = imageAtomicExchange(packedVoxelImg, index + ivec3(2, 0, 0), uint(0)).r;

	float radianceGreen = float((data & 0xFFFF0000) >> 16) / 0x7FF;
	float radianceBlue = float(data & 0xFFFF) / 0x7FF;

	data = imageAtomicExchange(packedVoxelImg, index + ivec3(3, 0, 0), uint(0)).r;

	float normalX = (float((data & 0xFFFF0000) >> 16) / 0x7FF);
	float normalY = (float(data & 0xFFFF) / 0x7FF);

	data = imageAtomicExchange(packedVoxelImg, index + ivec3(4, 0, 0), uint(0)).r;

	float normalZ = (float((data & 0xFFFF0000) >> 16) / 0x7FF);
	outRoughness = float(data & 0xFFFF) / 0xFF;

	outColor = vec3(colorRed, colorGreen, colorBlue);
	outRadiance = vec3(radianceRed, radianceGreen, radianceBlue);
	outNormal = vec3(normalX, normalY, normalZ);
	return float(imageAtomicExchange(packedVoxelImg, index + ivec3(5, 0, 0), uint(0)).r);
}

#endif
