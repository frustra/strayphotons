#ifndef VOXEL_TRACE_SHARED2_GLSL_INCLUDED
#define VOXEL_TRACE_SHARED2_GLSL_INCLUDED

##import raytrace/intersection
##import voxel_trace_shared

float GetVoxelNearest(vec3 position, int level, out vec3 radiance)
{
	vec4 radianceData = texelFetch(voxelRadiance, ivec3(position) >> level, level);
	radiance = radianceData.rgb;
	return radianceData.a;
}

float TraceVoxelGrid(int level, vec3 rayPos, vec3 rayDir, out vec3 hitRadiance)
{
	vec3 voxelVolumeMax = vec3(voxelSize * VoxelGridSize * 0.5);
	vec3 voxelVolumeMin = -voxelVolumeMax;

	rayPos -= voxelGridCenter;

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

	vec3 voxelPos = (rayPos.xyz / voxelSize + VoxelGridSize * 0.5);
	ivec3 voxelIndex = ivec3(voxelPos);

	vec3 deltaDist = abs(vec3(1.0) / rayDir);
	vec3 raySign = sign(rayDir);
	ivec3 rayStep = ivec3(raySign);

	// Distance to next voxel in each axis
	vec3 sideDist = (raySign * (vec3(voxelIndex) - voxelPos) + (raySign * 0.5) + 0.5) * deltaDist;
	int maxIterations = int(VoxelGridSize * 3);
	bvec3 mask;

	for (int i = 0; i < maxIterations; i++)
	{
		vec3 radiance;
		float alpha = GetVoxelNearest(voxelPos, level, radiance);
		if (alpha > 0)
		{
			hitRadiance = radiance;
			return alpha;
		}
		alpha = GetVoxelNearest(voxelPos + rayDir * 0.01, level, radiance);
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