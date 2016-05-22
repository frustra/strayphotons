#ifdef CUBE_SAMPLER
#undef shadowSampler
#define shadowSampler samplerCubeArray
#else
#undef shadowSampler
#define shadowSampler sampler2DArray
#endif

float sampleOcclusion(shadowSampler map, int i, vec3 rPosition, vec4 hPosition, vec3 position) {
#ifdef CUBE_SAMPLER
	vec4 coord = vec4(rPosition.xyz * vec3(-1, -1, 1), float(lightMapIndex[i]));
#else
	vec3 coord = vec3(position.xy, float(lightMapIndex[i]));
#endif

	float sampledDepth = texture(map, coord).a;
	float fragmentDepth = (length(hPosition.xyz) - lightNearZ[i]) / (lightRadius[i] - lightNearZ[i]);

#ifdef CUBE_SAMPLER
	return smoothstep(fragmentDepth - 0.0001, fragmentDepth, sampledDepth);
#else
	return step(0, -hPosition.z) * smoothstep(fragmentDepth - 0.0010, fragmentDepth - 0.0005, sampledDepth);
#endif
}

float directOcclusion(shadowSampler map, int i, vec3 wPosition, vec4 hPosition, vec3 position, mat2 rotation0) {
	vec3 rPosition = wPosition - lightPosition[i];
#ifdef USE_PCF
	vec2 sampleRadius = 5.0 / textureSize(map, 0).xy;
	mat2 rotation1 = mat2(0.707, 0.707, -0.707, 0.707) * rotation0;
	float occlusion = 0;

	#ifdef CUBE_SAMPLER
		sampleRadius /= lightNearZ[i];
		float lPlane = length(rPosition.xz);
		vec3 x = normalize(vec3(-rPosition.z, 0, rPosition.x) + step(0, -lPlane) * vec3(1.0, 0.0, 0.0));
		vec3 y = normalize(vec3(normalize(rPosition.xz) * (lPlane - 1), lPlane).xzy);
		mat3 cubeDir = mat3(x, y, rPosition);
	#endif

	for (int s = 0; s < 8; s++) {
		vec2 offset = SpiralOffsets[s] * sampleRadius;
		#ifdef CUBE_SAMPLER
			occlusion += sampleOcclusion(map, i, rPosition + cubeDir * vec3(rotation0 * offset, 0), hPosition, position);
			occlusion += sampleOcclusion(map, i, rPosition + cubeDir * vec3(rotation1 * offset, 0), hPosition, position);
		#else
			occlusion += sampleOcclusion(map, i, rPosition, hPosition, position + vec3(rotation0 * offset, 0));
			occlusion += sampleOcclusion(map, i, rPosition, hPosition, position + vec3(rotation1 * offset, 0));
		#endif
	}

	return min(occlusion / 16.0, 1);
#else
	return sampleOcclusion(map, i, rPosition, hPosition, position);
#endif
}
