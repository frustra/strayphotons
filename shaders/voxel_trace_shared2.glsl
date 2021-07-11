#ifndef VOXEL_TRACE_SHARED2_GLSL_INCLUDED
#define VOXEL_TRACE_SHARED2_GLSL_INCLUDED

##import voxel_trace_shared
##import voxel_shared

void aabbIntersectFast(vec3 P, vec3 R, vec3 invR, vec3 aabb1, vec3 aabb2, out float tmin, out float tmax)
{
	vec3 t1 = (aabb1 - P) * invR;
	vec3 t2 = (aabb2 - P) * invR;
	vec3 tmins = min(t1, t2);
	vec3 tmaxs = max(t1, t2);
	tmin = max(max(tmins.x, tmins.y), tmins.z);
	tmax = min(min(tmaxs.x, tmaxs.y), tmaxs.z);
}

float GetVoxelNearest(vec3 position, int level, int map, out vec3 radiance)
{
	vec4 radianceData;
	if (level > 0) {
		radianceData = texelFetch(voxelRadianceMips, (ivec3(position) + ivec3(map * VOXEL_GRID_SIZE, 0, 0)) >> level, level - 1);
	} else {
		radianceData = texelFetch(voxelRadiance, ivec3(position), 0);
	}
	radiance = radianceData.rgb;
	return radianceData.a;
}

float TraceVoxelGrid(int level, vec3 rayPos, vec3 rayDir, int map, out vec3 hitRadiance)
{
	vec3 voxelVolumeMax = vec3(voxelInfo.size * VOXEL_GRID_SIZE * 0.5);
	vec3 voxelVolumeMin = -voxelVolumeMax;

	rayPos -= voxelInfo.center;

	float tmin, tmax;
	aabbIntersectFast(rayPos, rayDir, 1.0 / rayDir, voxelVolumeMin, voxelVolumeMax, tmin, tmax);

	if (tmin > tmax || tmax < 0)
	{
		hitRadiance = vec3(0);
        return 0;
	}

	if (tmin > 0)
	{
		rayPos += rayDir * tmin;
	}

	vec3 voxelPos = (rayPos.xyz / voxelInfo.size + VOXEL_GRID_SIZE * 0.5);
	ivec3 voxelIndex = ivec3(voxelPos);

	vec3 deltaDist = abs(vec3(1.0) / rayDir);
	vec3 raySign = sign(rayDir);
	ivec3 rayStep = ivec3(raySign);

	// Distance to next voxel in each axis
	vec3 sideDist = (raySign * (vec3(voxelIndex) - voxelPos) + (raySign * 0.5) + 0.5) * deltaDist;
	int maxIterations = int(VOXEL_GRID_SIZE * 3);
	bvec3 mask;

	for (int i = 0; i < maxIterations; i++)
	{
		vec3 radiance;
		float alpha = GetVoxelNearest(voxelPos + rayDir * 0.001, level, map, radiance);
		if (alpha > 0)
		{
			hitRadiance = radiance;
			return alpha;
		}

		// Find axis with minimum distance
		mask = lessThanEqual(sideDist.xyz, min(sideDist.yzx, sideDist.zxy));
		voxelPos += rayDir * dot(vec3(mask), sideDist);
		voxelIndex += ivec3(mask) * rayStep;
		sideDist = (raySign * (vec3(voxelIndex) - voxelPos) + (raySign * 0.5) + 0.5) * deltaDist;
	}

	hitRadiance = vec3(0);
    return 0;
}

#endif