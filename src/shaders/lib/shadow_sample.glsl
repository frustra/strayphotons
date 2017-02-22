#ifdef MIRROR_SAMPLE
#define TEXTURE_SAMPLER(coord) mirrorShadowMap, vec3(coord, info.index)
#else
#undef TEXTURE_SAMPLER
#define TEXTURE_SAMPLER(coord) shadowMap, coord
#endif

struct ShadowInfo {
	int index;
	vec3 shadowMapPos;
	mat4 projMat, invProjMat;
	vec4 mapOffset;
	vec2 clip;
	vec4 nearInfo;
};

#ifdef MIRROR_SAMPLE
float SimpleOcclusionMirror(ShadowInfo info) {
#else
float SimpleOcclusion(ShadowInfo info) {
#endif
	vec3 texCoord = ViewPosToScreenPos(info.shadowMapPos, info.projMat);

	if (texCoord.xy != clamp(texCoord.xy, 0.0, 1.0)) return 0.0;

	float shadowBias = shadowBiasDistance / (info.clip.y - info.clip.x);

	float testDepth = LinearDepth(info.shadowMapPos, info.clip);
	float sampledDepth = texture(TEXTURE_SAMPLER(texCoord.xy * info.mapOffset.zw + info.mapOffset.xy)).r;

	return step(info.clip.x, -info.shadowMapPos.z) * smoothstep(testDepth - shadowBias, testDepth - shadowBias * 0.2, sampledDepth);
}

#ifdef MIRROR_SAMPLE
float SampleOcclusionMirror(vec3 shadowMapCoord, ShadowInfo info, vec3 surfaceNormal, vec2 offset) {
	vec2 mapSize = textureSize(mirrorShadowMap, 0).xy * info.mapOffset.zw;
#else
float SampleOcclusion(vec3 shadowMapCoord, ShadowInfo info, vec3 surfaceNormal, vec2 offset) {
	vec2 mapSize = textureSize(shadowMap, 0).xy * info.mapOffset.zw;
#endif
	vec2 texelSize = 1.0 / mapSize;
	vec3 texCoord = shadowMapCoord + vec3(offset * texelSize.xy, 0);

	if (texCoord.xy != clamp(texCoord.xy, 0.0, 1.0)) return 0.0;
	vec2 edgeTerm = smoothstep(vec2(0.0), texelSize, texCoord.xy);
	edgeTerm *= smoothstep(vec2(1.0), 1.0 - texelSize, texCoord.xy);

	vec3 rayDir = normalize(vec3(texCoord.xy * info.nearInfo.zw + info.nearInfo.xy, -info.clip.x));

	float t = dot(surfaceNormal, info.shadowMapPos) / dot(surfaceNormal, rayDir);
	vec3 hitPos = rayDir * t;

	float shadowBias = shadowBiasDistance / (info.clip.y - info.clip.x);

	float testDepth = LinearDepth(hitPos, info.clip);
	testDepth = max(testDepth, LinearDepth(info.shadowMapPos, info.clip) - shadowBias * 2.0);

	vec2 coord = texCoord.xy * info.mapOffset.zw + info.mapOffset.xy;
	vec3 sampledDepth = vec3(texture(TEXTURE_SAMPLER(coord)).r, 0, 0);
	vec4 values = textureGather(TEXTURE_SAMPLER(coord), 0);
	sampledDepth.y = min(values.x, min(values.y, min(values.z, values.w)));
	sampledDepth.z = max(values.x, max(values.y, max(values.z, values.w)));

	float minTest = min(sampledDepth.y, testDepth - shadowBias);
	minTest = max(minTest, step(sampledDepth.z, testDepth - shadowBias * 2.0) * testDepth - shadowBias);

	return step(info.clip.x, -info.shadowMapPos.z) * edgeTerm.x * edgeTerm.y * smoothstep(0.0, 0.2, -dot(surfaceNormal, rayDir)) * smoothstep(minTest, testDepth, sampledDepth.x);
}

#ifdef MIRROR_SAMPLE
float DirectOcclusionMirror(ShadowInfo info, vec3 surfaceNormal, mat2 rotation0) {
#else
float DirectOcclusion(ShadowInfo info, vec3 surfaceNormal, mat2 rotation0) {
#endif
	vec3 shadowMapCoord = ViewPosToScreenPos(info.shadowMapPos, info.projMat);

	float occlusion = 0;
	for (int x = -DiskKernelRadius; x <= DiskKernelRadius; x++) {
		for (int y = -DiskKernelRadius; y <= DiskKernelRadius; y++) {
			vec2 offset = vec2(x, y);

#ifdef MIRROR_SAMPLE
			occlusion += SampleOcclusionMirror(shadowMapCoord, info, surfaceNormal, rotation0 * offset)
						 * DiskKernel[x + DiskKernelRadius][y + DiskKernelRadius];
#else
			occlusion += SampleOcclusion(shadowMapCoord, info, surfaceNormal, rotation0 * offset)
						 * DiskKernel[x + DiskKernelRadius][y + DiskKernelRadius];
#endif
		}
	}

	return smoothstep(0.3, 1.0, occlusion);
}
