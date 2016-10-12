#ifndef VOXEL_SHARED_GLSL_INCLUDED
#define VOXEL_SHARED_GLSL_INCLUDED

##import raytrace/intersection

const float VoxelGridSize = 256;
const float VoxelSize = 0.12;
const vec3 VoxelGridCenter = vec3(0, 5, 0);

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

bool GetVoxel(sampler3D colors, sampler3D normals, vec3 position, int level, out vec4 color, out vec3 normal)
{
	vec4 colorData = textureLod(colors, position/vec3(int(VoxelGridSize)>>level), level);
	vec4 normalData = textureLod(normals, position/vec3(int(VoxelGridSize)>>level), level);
	color = vec4(colorData.rgb / (normalData.a + 0.00001), colorData.a);
	normal = normalData.rgb;
	return colorData.a > 0;
}

vec4 SampleVoxel(sampler3D colors, sampler3D normals, vec3 position, float size)
{
	float level = max(0, log2(size));
	vec4 colorData = textureLod(colors, position/vec3(VoxelGridSize), level);
	vec4 normalData = textureLod(normals, position/vec3(VoxelGridSize), level);
	return vec4(colorData.rgb / (normalData.a + 0.00001), colorData.a);
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

void TraceVoxelGrid(sampler3D colors, sampler3D normals, int level, vec3 rayPos, vec3 rayDir, out vec3 hitColor, out vec3 hitNormal)
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
		vec4 color;
		vec3 normal;
		if (GetVoxel(colors, normals, voxelPos, level, color, normal))
		{
			hitColor = color.rgb;
			hitNormal = normal;
			return;
		}

		// Find axis with minimum distance
		mask = lessThanEqual(sideDist.xyz, min(sideDist.yzx, sideDist.zxy));
		voxelPos += rayDir * invScale;// * dot(vec3(mask), sideDist);
		voxelIndex += ivec3(mask) * rayStep;
		sideDist = (raySign * (vec3(voxelIndex) - voxelPos) + (raySign * 0.5) + 0.5) * deltaDist;
	}

	hitColor = vec3(0);
}

vec4 ConeTraceGrid(sampler3D colors, sampler3D normals, float ratio, vec3 rayPos, vec3 rayDir, vec3 surfaceNormal)
{
	vec3 voxelPos = (rayPos.xyz / VoxelSize + VoxelGridSize * 0.5);
	float dist = max(1.75, min(1.75 / dot(rayDir, surfaceNormal), VoxelGridSize / 2));
	float maxDist = VoxelGridSize * 1.5;

	vec4 result = vec4(0);

	while (dist < maxDist && result.a < 0.9999)
	{
		float size = max(0.5, ratio * dist);
		float planeDist = dot(surfaceNormal, rayDir * dist) - 1.75;
		// If the sample intersects the surface, move it over
		float offset = max(0, size - planeDist);
		vec4 value = SampleVoxel(colors, normals, voxelPos + rayDir * dist + offset * surfaceNormal, size);
		result += vec4(value.rgb * value.a, value.a) * (1.2 - result.a) * (1 - step(0, -value.a));

		dist += size;
	}

	return vec4(result.rgb, result.a);
}

#endif
