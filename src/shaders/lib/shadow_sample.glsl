float sampleOcclusion(sampler2D map, int i, vec3 shadowMapPos, vec3 shadowMapTexCoord) {
	float sampledDepth = texture(map, shadowMapTexCoord.xy * lightMapOffset[i].zw + lightMapOffset[i].xy).r;
	float fragmentDepth = (length(shadowMapPos) - lightClip[i].x) / (lightClip[i].y - lightClip[i].x);

	return step(0, -shadowMapPos.z) * smoothstep(fragmentDepth - 0.0001, fragmentDepth - 0.00005, sampledDepth);
}

float directOcclusion(sampler2D map, int i, vec3 shadowMapPos, vec3 shadowMapTexCoord, mat2 rotation0) {
#ifdef USE_PCF
	vec2 sampleRadius = 5.0 / textureSize(map, 0).xy;
	mat2 rotation1 = mat2(0.707, 0.707, -0.707, 0.707) * rotation0;
	float occlusion = 0;

	for (int s = 0; s < 8; s++) {
		vec2 offset = SpiralOffsets[s] * sampleRadius;

		occlusion += sampleOcclusion(map, i, shadowMapPos, shadowMapTexCoord + vec3(rotation0 * offset, 0));
		occlusion += sampleOcclusion(map, i, shadowMapPos, shadowMapTexCoord + vec3(rotation1 * offset, 0));
	}

	return min(occlusion / 16.0, 1);
#else
	return sampleOcclusion(map, i, shadowMapPos, shadowMapTexCoord);
#endif
}
