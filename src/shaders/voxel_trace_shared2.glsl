#ifndef VOXEL_TRACE_SHARED2_GLSL_INCLUDED
#define VOXEL_TRACE_SHARED2_GLSL_INCLUDED

##import raytrace/intersection
##import voxel_trace_shared

const float VoxelEps = 0.00001;

float GetVoxel(vec3 position, int level, out vec3 color, out vec3 normal, out vec3 radiance, out float roughness)
{
	vec3 invMipVoxelGridSize = vec3(1.0 / float(int(VoxelGridSize)>>level));

	vec4 colorData = textureLod(voxelColor, position * InvVoxelGridSize, 0);
	vec4 normalData = textureLod(voxelNormal, position * InvVoxelGridSize, 0);
	vec4 radianceData = textureLod(voxelRadiance, position * invMipVoxelGridSize, level);

	color = colorData.rgb / (colorData.a + VoxelEps);
	normal = normalize(normalData.xyz / (colorData.a + VoxelEps));
	radiance = radianceData.rgb / (radianceData.a + VoxelEps);
	roughness = normalData.a / (colorData.a + VoxelEps);

	return radianceData.a;
}

float TraceVoxelGrid(int level, vec3 rayPos, vec3 rayDir, out vec3 hitColor, out vec3 hitNormal, out vec3 hitRadiance, out float hitRoughness)
{
	float scale = float(1 << level);
	float invScale = 1.0 / scale;

	vec3 voxelVolumeMax = vec3(voxelSize * VoxelGridSize * 0.5);
	vec3 voxelVolumeMin = -voxelVolumeMax;

	rayPos -= voxelGridCenter;

	float tmin, tmax;
	aabbIntersectFast(rayPos, rayDir, 1.0 / rayDir, voxelVolumeMin, voxelVolumeMax, tmin, tmax);

	if (tmin > tmax || tmax < 0)
	{
		hitColor = vec3(0);
		hitNormal = vec3(0);
		hitRadiance = vec3(0);
		hitRoughness = 0;
        return 0;
	}

	if (tmin > 0)
	{
		rayPos += rayDir * tmin;
	}

	vec3 voxelPos = (rayPos.xyz / voxelSize + VoxelGridSize * 0.5) * invScale;

	vec4 rng = randState(rayPos);
	voxelPos += rayDir * rand2(rng);

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
		vec3 color, normal, radiance;
		float roughness;
		float alpha = GetVoxel(voxelPos, level, color, normal, radiance, roughness);
		if (alpha > 0)
		{
			hitColor = color;
			hitNormal = normal;
			hitRadiance = radiance;
			hitRoughness = roughness;
			return alpha;
		}

		// Find axis with minimum distance
		mask = lessThanEqual(sideDist.xyz, min(sideDist.yzx, sideDist.zxy));
		voxelPos += rayDir;// * dot(vec3(mask), sideDist);
		voxelIndex += ivec3(mask) * rayStep;
		sideDist = (raySign * (vec3(voxelIndex) - voxelPos) + (raySign * 0.5) + 0.5) * deltaDist;
	}

	hitColor = vec3(0);
	hitNormal = vec3(0);
	hitRadiance = vec3(0);
	hitRoughness = 0;
    return 0;
}

#endif