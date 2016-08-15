#ifndef VOXEL_SHARED_GLSL_INCLUDED
#define VOXEL_SHARED_GLSL_INCLUDED

##import raytrace/intersection

const float VoxelGridSize = 256;
const float VoxelSize = 0.08;
const vec3 VoxelGridCenter = vec3(0, 5, 0);

const uint MaxFragListMask = 8191;
const uint FragListWidthBits = 13;
const uint MipmapWorkGroupSize = 256;

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

bool GetVoxel(sampler3D unpackedVoxelTex, ivec3 position, int level, out vec3 color)
{
	vec4 data = texelFetch(unpackedVoxelTex, position, level);
	color = data.rgb;
	return data.a > 0;
}

bool UnpackVoxel(usampler3D packedVoxelTex, ivec3 position, out vec4 color)
{
	ivec3 index = position * ivec3(2, 1, 1);
	uint data = texelFetch(packedVoxelTex, index + ivec3(1, 0, 0), 0).r;
	uint count = data & 0xFFFF;

	if (count > 0) {
		float blue = float((data & 0xFFFF0000) >> 16) / 255.0;

		data = texelFetch(packedVoxelTex, index, 0).r;
		float red = float((data & 0xFFFF0000) >> 16) / 255.0;
		float green = float(data & 0xFFFF) / 255.0;
		color.rgb = vec3(red, green, blue) / float(count);
		color.a = float(count);
		return true;
	}

	return false;
}

bool UnpackVoxel(layout(r32ui) uimage3D packedVoxelImg, ivec3 position, out vec4 color)
{
	ivec3 index = position * ivec3(2, 1, 1);
	uint data = imageLoad(packedVoxelImg, index + ivec3(1, 0, 0)).r;
	uint count = data & 0xFFFF;

	if (count > 0) {
		float blue = float((data & 0xFFFF0000) >> 16) / 255.0;

		data = imageLoad(packedVoxelImg, index).r;
		float red = float((data & 0xFFFF0000) >> 16) / 255.0;
		float green = float(data & 0xFFFF) / 255.0;

		color.rgb = vec3(red, green, blue) / float(count);
		color.a = float(count);
		return true;
	}

	return false;
}

vec4 ReadVoxel(layout(r32ui) uimage3D packedVoxelImg, ivec3 position)
{
	ivec3 index = position * ivec3(2, 1, 1);

	uint data = imageLoad(packedVoxelImg, index).r;
	float red = float((data & 0xFFFF0000) >> 16) / 255.0;
	float green = float(data & 0xFFFF) / 255.0;

	data = imageLoad(packedVoxelImg, index + ivec3(1, 0, 0)).r;

	float blue = float((data & 0xFFFF0000) >> 16) / 255.0;
	float alpha = float(data & 0xFFFF);

	return vec4(red, green, blue, alpha);
}

vec4 ReadVoxelAndClear(layout(r32ui) uimage3D packedVoxelImg, ivec3 position)
{
	ivec3 index = position * ivec3(2, 1, 1);

	uint data = imageAtomicExchange(packedVoxelImg, index, uint(0)).r;
	float red = float((data & 0xFFFF0000) >> 16) / 255.0;
	float green = float(data & 0xFFFF) / 255.0;

	data = imageAtomicExchange(packedVoxelImg, index + ivec3(1, 0, 0), uint(0)).r;

	float blue = float((data & 0xFFFF0000) >> 16) / 255.0;
	float alpha = float(data & 0xFFFF);

	return vec4(red, green, blue, alpha);
}

bool UnpackVoxelAndClear(layout(r32ui) uimage3D packedVoxelImg, ivec3 position, out vec4 color)
{
	ivec3 index = position * ivec3(2, 1, 1);
	uint data = imageAtomicExchange(packedVoxelImg, index + ivec3(1, 0, 0), uint(0));
	uint count = data & 0xFFFF;

	if (count > 0) {
		float blue = float((data & 0xFFFF0000) >> 16) / 255.0;

		data = imageAtomicExchange(packedVoxelImg, index, uint(0));
		float red = float((data & 0xFFFF0000) >> 16) / 255.0;
		float green = float(data & 0xFFFF) / 255.0;

		color.rgb = vec3(red, green, blue) / float(count);
		color.a = float(count);
		return true;
	}

	return false;
}

void TraceVoxelGrid(sampler3D voxels, int level, vec3 rayPos, vec3 rayDir, out vec3 hitColor)
{
	float scale = float(1 << level);
	float invScale = 1.0 / scale;

	vec3 voxelVolumeMax = vec3(VoxelSize * VoxelGridSize * 0.5);
	vec3 voxelVolumeMin = -voxelVolumeMax;

	float tmin, tmax;
	aabbIntersectFast(rayPos, rayDir, 1.0 / rayDir, voxelVolumeMin, voxelVolumeMax, tmin, tmax);

	if (tmin > tmax || tmax < 0)
	{
		hitColor = vec3(0);
        return;
	}

	if (tmin > 0)
	{
		rayPos += rayDir * tmin;
	}

	vec3 voxelPos = (rayPos.xyz / VoxelSize + VoxelGridSize * 0.5) * invScale;
	ivec3 voxelIndex = ivec3(voxelPos);

	vec3 deltaDist = abs(vec3(scale) / rayDir);
	vec3 raySign = sign(rayDir);
	ivec3 rayStep = ivec3(raySign);

	// Distance to next voxel in each axis
	vec3 sideDist = (raySign * (vec3(voxelIndex) - voxelPos) + (raySign * 0.5) + 0.5) * deltaDist;
	int maxIterations = int((VoxelGridSize * 3) * invScale);
	bvec3 mask;

	for (int i = 0; i < maxIterations; i++)
	{
		vec3 color;
		if (GetVoxel(voxels, voxelIndex, level, color))
		{
			hitColor = color;
			return;
		}

		// Find axis with minimum distance
		mask = lessThanEqual(sideDist.xyz, min(sideDist.yzx, sideDist.zxy));
		sideDist += vec3(mask) * deltaDist;
		voxelIndex += ivec3(mask) * rayStep;
	}

	hitColor = vec3(0);
}

#endif
