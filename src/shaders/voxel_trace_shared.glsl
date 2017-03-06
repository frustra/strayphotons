#ifndef VOXEL_TRACE_SHARED_GLSL_INCLUDED
#define VOXEL_TRACE_SHARED_GLSL_INCLUDED

##import lib/spatial_util

const float InvVoxelGridSize = 1.0 / VoxelGridSize;

vec4 SampleVoxelLod(vec3 position, vec3 dir, float level)
{
	return textureLod(voxelRadiance, position * InvVoxelGridSize, level) * vec4(vec3(VoxelFixedPointExposure), 1.0);
}

vec4 ConeTraceGrid(float ratio, vec3 rayPos, vec3 rayDir, vec3 surfaceNormal, vec2 fragCoord)
{
	vec3 voxelPos = (rayPos.xyz - voxelGridCenter) / voxelSize + VoxelGridSize * 0.5;

	float dist = InterleavedGradientNoise(fragCoord);
	float maxDist = VoxelGridSize * 1.5;

	vec4 result = vec4(0);

	while (dist < maxDist && result.a < 0.9999)
	{
		float size = max(1.0, ratio * dist);
		float planeDist = dot(surfaceNormal, rayDir * dist) - 1.75;
		// If the sample intersects the surface, move it over
		float offset = max(0, size - planeDist);
		vec3 position = voxelPos + rayDir * dist;

		float level = max(0, log2(size));
		vec4 value = SampleVoxelLod(position + offset * surfaceNormal, rayDir, level);
		// Bias the alpha to prevent traces going through objects.
		value.a = smoothstep(0.0, 0.4, value.a);
		result += vec4(value.rgb, value.a) * (1.0 - result.a) * (1 - step(0, -value.a));

		dist += size;
	}

	if (dist >= maxDist) dist = -1;
	return vec4(result.rgb, dist * voxelSize);
}

vec4 ConeTraceGridDiffuse(vec3 rayPos, vec3 rayDir, vec3 surfaceNormal)
{
	vec3 voxelPos = (rayPos.xyz - voxelGridCenter) / voxelSize + VoxelGridSize * 0.5;
	float startDist = max(1.75, min(1.75 / dot(rayDir, surfaceNormal), 3.0));
	float dist = startDist;
	float maxDist = VoxelGridSize * 1.5;

	vec4 result = vec4(0);
	float level = 0;

	while (dist < maxDist && result.a < 0.9999)
	{
		vec4 value = SampleVoxelLod(voxelPos + rayDir * dist, rayDir, level);
		result += vec4(value.rgb, value.a) * (1.0 - result.a) * (1 - step(0, -value.a));

		level += 1.0;
		dist *= 2.0;
	}

	return result;
}

vec3 HemisphereIndirectDiffuse(vec3 worldPosition, vec3 worldNormal, vec2 fragCoord) {
	float rOffset = InterleavedGradientNoise(fragCoord);

	vec3 sampleDir = OrientByNormal(rOffset * M_PI * 2.0, 0.1, worldNormal);
	vec4 indirectDiffuse = ConeTraceGridDiffuse(worldPosition, sampleDir, worldNormal);

	for (int a = 3; a <= 6; a += 3) {
		float diffuseScale = 1.0 / a;
		for (float r = 0; r < a; r++) {
			vec3 sampleDir = OrientByNormal((r + rOffset) * diffuseScale * M_PI * 2.0, a * 0.1, worldNormal);
			vec4 sampleColor = ConeTraceGridDiffuse(worldPosition, sampleDir, worldNormal);

			indirectDiffuse += sampleColor * dot(sampleDir, worldNormal) * diffuseScale * 0.5;
		}
	}

	return indirectDiffuse.rgb * 0.5;
}

#endif
