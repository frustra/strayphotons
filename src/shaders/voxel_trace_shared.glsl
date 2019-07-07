#ifndef VOXEL_TRACE_SHARED_GLSL_INCLUDED
#define VOXEL_TRACE_SHARED_GLSL_INCLUDED

##import lib/spatial_util

const float InvVoxelGridSize = 1.0 / VOXEL_GRID_SIZE;

vec4 SampleVoxelLod(vec3 position, float level, int map)
{
	vec3 scale = InvVoxelGridSize / vec3(MAX_VOXEL_AREAS, 1, 1);
	vec3 mipCoord = position * scale;
	vec3 mipDelta = vec3(VOXEL_GRID_SIZE, 0, 0) * scale;

	vec4 result;
	if (level >= 1) {
		result = textureLod(voxelRadianceMips, mipCoord + mipDelta * map, level - 1);
	} else {
		// TODO(xthexder): Evauluate if this is worth the extra lookup
		vec4 a = textureLod(voxelRadiance, position * InvVoxelGridSize, 0);
		a.a = min(a.a, 1.0);
		vec4 b = textureLod(voxelRadianceMips, mipCoord + mipDelta * map, 0);
		result = mix(a, b, level);
	}
	return result;
}

vec4 SampleVoxelLodDiffuse(vec3 position, float level, int map)
{
	vec3 scale = InvVoxelGridSize / vec3(MAX_VOXEL_AREAS, 1, 1);
	vec3 mipCoord = position * scale;
	vec3 mipDelta = vec3(VOXEL_GRID_SIZE, 0, 0) * scale;

	vec4 result;
	if (level >= 1) {
		result = textureLod(voxelRadianceMips, mipCoord + mipDelta * map, level - 1);
	} else {
		result = textureLod(voxelRadiance, position * InvVoxelGridSize, 0);
	}
	result.a = min(result.a, 1.0);
	return result;
}

int GetMapForPoint(vec3 position)
{
	int j = 0;
	for (int i = 0; i < MAX_VOXEL_AREAS; i++) {
		if (position == clamp(position, voxelInfo.areas[i].areaMin, voxelInfo.areas[i].areaMax)) {
			j = i;
		}
	}
	return j;
}

vec4 ConeTraceGrid(float ratio, vec3 rayPos, vec3 rayDir, vec3 surfaceNormal, vec2 fragCoord)
{
	vec3 voxelPos = (rayPos.xyz - voxelInfo.center) / voxelInfo.size + VOXEL_GRID_SIZE * 0.5;

	float dist = InterleavedGradientNoise(fragCoord);
	vec4 result = vec4(0);

	for (int i = 0; i < VOXEL_GRID_SIZE; i++) {
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

		if (result.a > 0.999) break;
		dist += size;
	}

	return result;
}

vec4 ConeTraceGridDiffuse(vec3 rayPos, vec3 rayDir)
{
	vec3 voxelPos = (rayPos.xyz - voxelInfo.center) / voxelInfo.size + VOXEL_GRID_SIZE * 0.5;
	float startDist = 1.75;
	float dist = startDist;

	vec4 result = vec4(0);
	float level = 0;

	for (int level = 0; level < VOXEL_MIP_LEVELS; level++) {
		vec4 value = SampleVoxelLodDiffuse(voxelPos + rayDir * dist, float(level), GetMapForPoint(rayPos));
		result += vec4(value.rgb, value.a) * (1.0 - result.a) * (1 - step(0, -value.a));

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
