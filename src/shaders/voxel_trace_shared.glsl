#ifndef VOXEL_TRACE_SHARED_GLSL_INCLUDED
#define VOXEL_TRACE_SHARED_GLSL_INCLUDED

##import lib/spatial_util

const float InvVoxelGridSize = 1.0 / VOXEL_GRID_SIZE;

vec4 SampleVoxelLod(vec3 position, float level, int map)
{
	vec4 result;
	if (level >= 1) {
		vec3 scale = InvVoxelGridSize / vec3(MAX_VOXEL_AREAS + 1, 1, 1);
		vec3 mipCoord = position * scale;
		vec3 mipDelta = vec3(VOXEL_GRID_SIZE, 0, 0) * scale;

		result = textureLod(voxelRadianceMips, mipCoord + mipDelta * map, level - 1);
	} else {
		result = textureLod(voxelRadiance, position * InvVoxelGridSize, level);
	}
	return result * vec4(vec3(VoxelFixedPointExposure), 1.0);
}

int GetMapForPoint(vec3 position)
{
	for (int i = 0; i < MAX_VOXEL_AREAS; i++) {
		if (position == clamp(position, voxelInfo.areas[i].areaMin, voxelInfo.areas[i].areaMax)) {
			return i + 1;
		}
	}
	return 0;
}

vec4 ConeTraceGrid(float ratio, vec3 rayPos, vec3 rayDir, vec3 surfaceNormal, vec2 fragCoord)
{
	vec3 voxelPos = (rayPos.xyz - voxelInfo.center) / voxelInfo.size + VOXEL_GRID_SIZE * 0.5;

	float dist = InterleavedGradientNoise(fragCoord);
	float maxDist = VOXEL_GRID_SIZE * 1.5;

	vec4 result = vec4(0);

	while (dist < maxDist && result.a < 0.9999)
	{
		float size = max(1.0, ratio * dist);
		float planeDist = dot(surfaceNormal, rayDir * dist) - 1.75;
		// If the sample intersects the surface, move it over
		float offset = max(0, size - planeDist);
		vec3 position = voxelPos + rayDir * dist;

		float level = max(0, log2(size));
		vec4 value = SampleVoxelLod(position + offset * surfaceNormal, level, GetMapForPoint(rayPos));
		// Bias the alpha to prevent traces going through objects.
		value.a = smoothstep(0.0, 0.4, value.a);
		result += vec4(value.rgb, value.a) * (1.0 - result.a) * (1 - step(0, -value.a));

		dist += size;
	}

	if (dist >= maxDist) dist = -1;
	return vec4(result.rgb, dist * voxelInfo.size);
}

vec4 ConeTraceGridDiffuse(vec3 rayPos, vec3 rayDir)
{
	vec3 voxelPos = (rayPos.xyz - voxelInfo.center) / voxelInfo.size + VOXEL_GRID_SIZE * 0.5;
	float startDist = 1.75;
	float dist = startDist;

	vec4 result = vec4(0);
	float level = 0;

	for (int level = 0; level < VOXEL_MIP_LEVELS; level++) {
		vec4 value = SampleVoxelLod(voxelPos + rayDir * dist, float(level), GetMapForPoint(rayPos));
		result += vec4(value.rgb, value.a) * (1.0 - result.a) * (1 - step(0, -value.a));

		if (result.a > 0.999) break;
		dist *= 2.0;
	}

	return result;
}

vec3 HemisphereIndirectDiffuse(vec3 worldPosition, vec3 worldNormal, vec2 fragCoord) {
	float rOffset = InterleavedGradientNoise(fragCoord);

	vec3 sampleDir = OrientByNormal(rOffset * M_PI * 2.0, 0.1, worldNormal);
	vec4 indirectDiffuse = ConeTraceGridDiffuse(worldPosition, sampleDir);

	for (int a = 3; a <= 6; a += 3) {
		float diffuseScale = 1.0 / a;
		for (float r = 0; r < a; r++) {
			vec3 sampleDir = OrientByNormal((r + rOffset) * diffuseScale * M_PI * 2.0, a * 0.1, worldNormal);
			vec4 sampleColor = ConeTraceGridDiffuse(worldPosition, sampleDir);

			indirectDiffuse += sampleColor * dot(sampleDir, worldNormal) * diffuseScale * 0.5;
		}
	}

	return indirectDiffuse.rgb * 0.5;
}

#endif
