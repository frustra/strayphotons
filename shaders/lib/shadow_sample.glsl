#undef TEXTURE_SAMPLER
#ifdef MIRROR_SAMPLE
    #define TEXTURE_SAMPLER(coord) mirrorShadowMap, vec3(coord, info.index)
#else
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

    return smoothstep(testDepth - shadowBias, testDepth - shadowBias * 0.2, sampledDepth);
}

#ifdef MIRROR_SAMPLE
float SampleOcclusionMirror(vec3 shadowMapCoord, ShadowInfo info, vec3 surfaceNormal, mat2 rotation0) {
    vec2 mapSize = textureSize(mirrorShadowMap, 0).xy * info.mapOffset.zw;
#else
float SampleOcclusion(vec3 shadowMapCoord, ShadowInfo info, vec3 surfaceNormal, mat2 rotation0) {
    vec2 mapSize = textureSize(shadowMap, 0).xy * info.mapOffset.zw;
#endif
    vec2 texelSize = 1.0 / mapSize;
    vec3 texCoord = shadowMapCoord;// + vec3(rotation0 * texelSize.xy, 0);

	// Clip and smooth out the edges of the shadow map
    if (texCoord.xy != clamp(texCoord.xy, 0.0, 1.0)) return 0.0;
    // vec2 edgeTerm = smoothstep(vec2(0.0), texelSize, texCoord.xy);
    // edgeTerm *= smoothstep(vec2(1.0), 1.0 - texelSize, texCoord.xy);

	// Calculate the min and max shadow map positions in a 4x4 square around the current position
    vec2 coord = texCoord.xy * info.mapOffset.zw + info.mapOffset.xy;
	// vec4 values[4];
    // values[0] = textureGatherOffset(TEXTURE_SAMPLER(coord), ivec2(-1, 1));
    // values[1] = textureGatherOffset(TEXTURE_SAMPLER(coord), ivec2(1, 1));
    // values[2] = textureGatherOffset(TEXTURE_SAMPLER(coord), ivec2(-1, -1));
    // values[3] = textureGatherOffset(TEXTURE_SAMPLER(coord), ivec2(1, -1));

	// // Cut out corners to make it a vaugely circular sample pattern
	// values[0].x = values[0].y;
	// values[1].y = values[1].x;
	// values[2].z = values[2].w;
	// values[3].w = values[3].z;

	// vec2 minMax;
    // minMax.x = min(values[0].x, min(values[0].y, min(values[0].z, values[0].w)));
    // minMax.x = min(minMax.x, min(values[1].x, min(values[1].y, min(values[1].z, values[1].w))));
    // minMax.x = min(minMax.x, min(values[2].x, min(values[2].y, min(values[2].z, values[2].w))));
    // minMax.x = min(minMax.x, min(values[3].x, min(values[3].y, min(values[3].z, values[3].w))));

    // minMax.y = max(values[0].x, max(values[0].y, max(values[0].z, values[0].w)));
    // minMax.y = max(minMax.y, max(values[1].x, max(values[1].y, max(values[1].z, values[1].w))));
    // minMax.y = max(minMax.y, max(values[2].x, max(values[2].y, max(values[2].z, values[2].w))));
    // minMax.y = max(minMax.y, max(values[3].x, max(values[3].y, max(values[3].z, values[3].w))));
	
	vec4 values;
	// values = textureGather(TEXTURE_SAMPLER(coord));
    // values.x = textureOffset(TEXTURE_SAMPLER(coord), ivec2(-1, -1)).r;
    // values.y = textureOffset(TEXTURE_SAMPLER(coord), ivec2(1, -1)).r;
    // values.z = textureOffset(TEXTURE_SAMPLER(coord), ivec2(-1, 1)).r;
    // values.w = textureOffset(TEXTURE_SAMPLER(coord), ivec2(1, 1)).r;
	// values.x = texture(TEXTURE_SAMPLER((shadowMapCoord.xy + rotation0 * vec2(-texelSize.x, -texelSize.y)) * info.mapOffset.zw + info.mapOffset.xy)).r;
	// values.y = texture(TEXTURE_SAMPLER((shadowMapCoord.xy + rotation0 * vec2(texelSize.x, -texelSize.y)) * info.mapOffset.zw + info.mapOffset.xy)).r;
	// values.z = texture(TEXTURE_SAMPLER((shadowMapCoord.xy + rotation0 * vec2(-texelSize.x, texelSize.y)) * info.mapOffset.zw + info.mapOffset.xy)).r;
	// values.w = texture(TEXTURE_SAMPLER((shadowMapCoord.xy + rotation0 * vec2(texelSize.x, texelSize.y)) * info.mapOffset.zw + info.mapOffset.xy)).r;
	
    values.x = textureOffset(TEXTURE_SAMPLER(coord), ivec2(-1, 0)).r;
    values.y = textureOffset(TEXTURE_SAMPLER(coord), ivec2(1, 0)).r;
    values.z = textureOffset(TEXTURE_SAMPLER(coord), ivec2(0, -1)).r;
    values.w = textureOffset(TEXTURE_SAMPLER(coord), ivec2(0, 1)).r;
	// values.x = texture(TEXTURE_SAMPLER((shadowMapCoord.xy + /*rotation0 **/ vec2(-texelSize.x, 0)) * info.mapOffset.zw + info.mapOffset.xy)).r;
	// values.y = texture(TEXTURE_SAMPLER((shadowMapCoord.xy + /*rotation0 **/ vec2(texelSize.x, 0)) * info.mapOffset.zw + info.mapOffset.xy)).r;
	// values.z = texture(TEXTURE_SAMPLER((shadowMapCoord.xy + /*rotation0 **/ vec2(0, -texelSize.y)) * info.mapOffset.zw + info.mapOffset.xy)).r;
	// values.w = texture(TEXTURE_SAMPLER((shadowMapCoord.xy + /*rotation0 **/ vec2(0, texelSize.y)) * info.mapOffset.zw + info.mapOffset.xy)).r;

	vec2 minMax;
    minMax.x = min(values.x, min(values.y, min(values.z, values.w)));
    minMax.y = max(values.x, max(values.y, max(values.z, values.w)));

    vec2 faceSlopeOffset = texelSize * normalize(ViewPosToScreenPos(info.shadowMapPos + surfaceNormal, info.projMat).xy - shadowMapCoord.xy);
    vec3 rayDir = normalize(vec3(shadowMapCoord.xy * info.nearInfo.zw + info.nearInfo.xy, -info.clip.x));
    vec3 rayDir2 = normalize(vec3((shadowMapCoord.xy + faceSlopeOffset) * info.nearInfo.zw + info.nearInfo.xy, -info.clip.x));

    float t = dot(surfaceNormal, info.shadowMapPos) / dot(surfaceNormal, rayDir);
    float t2 = dot(surfaceNormal, info.shadowMapPos) / dot(surfaceNormal, rayDir2);
    vec3 hitPos = rayDir * t;
    vec3 hitPos2 = rayDir2 * t2;

	// float faceSlope = dot(surfaceNormal, -normalize(info.shadowMapPos));
	// faceSlope = sqrt(1 - (faceSlope * faceSlope));

	// float shadowBias = 0.001 + abs(LinearDepth(hitPos2, info.clip) - LinearDepth(hitPos, info.clip)); // max(texelSize.x, texelSize.y) * faceSlope + 0.001;
    float shadowBias = shadowBiasDistance / (info.clip.y - info.clip.x);

    float testDepth = LinearDepth(hitPos, info.clip);
    // testDepth = max(testDepth, LinearDepth(info.shadowMapPos, info.clip) - shadowBias * 2.0);

    // float sampledDepth = texture(TEXTURE_SAMPLER(coord)).r;
    // float minTest = min(minMax.x, testDepth - shadowBias);
    // minTest = max(minTest, step(minMax.y, testDepth - shadowBias * 2.0) * testDepth - shadowBias);
    // float minTest = min(minMax.x, testDepth);
    // minTest = max(minTest, step(minMax.y, testDepth) * testDepth - shadowBias);

    // return step(abs(sampledDepth - (minMax.x + minMax.y) * 0.5), shadowBias * 0.5) * step(testDepth, sampledDepth + shadowBias)
    // return abs(values.x - values.y);
	return (1 - linstep(minMax.x, minMax.y, testDepth - shadowBias));// * faceSlope;
	// return edgeTerm.x * edgeTerm.y * smoothstep(minTest, testDepth, sampledDepth);
}

#ifdef MIRROR_SAMPLE
float DirectOcclusionMirror(ShadowInfo info, vec3 surfaceNormal, mat2 rotation0) {
#else
float DirectOcclusion(ShadowInfo info, vec3 surfaceNormal, mat2 rotation0) {
#endif
    vec3 shadowMapCoord = ViewPosToScreenPos(info.shadowMapPos, info.projMat);

#ifdef MIRROR_SAMPLE
    return SampleOcclusionMirror(shadowMapCoord, info, surfaceNormal, rotation0);
#else
    return SampleOcclusion(shadowMapCoord, info, surfaceNormal, rotation0);
#endif

//     float occlusion = 0;
//     for (int x = -DiskKernelRadius; x <= DiskKernelRadius; x++) {
//         for (int y = -DiskKernelRadius; y <= DiskKernelRadius; y++) {
//             vec2 offset = vec2(x, y) * 1.3;

// #ifdef MIRROR_SAMPLE
//             occlusion += SampleOcclusionMirror(shadowMapCoord, info, surfaceNormal, rotation0 * offset) *
//                          DiskKernel[x + DiskKernelRadius][y + DiskKernelRadius];
// #else
//             occlusion += SampleOcclusion(shadowMapCoord, info, surfaceNormal, rotation0 * offset) *
//                          DiskKernel[x + DiskKernelRadius][y + DiskKernelRadius];
// #endif
//         }
//     }

//     return smoothstep(0.3, 1.0, occlusion);
}

float SampleVarianceShadowMap(ShadowInfo info, float varianceMin, float lightBleedReduction) {
    vec3 texCoord = ViewPosToScreenPos(info.shadowMapPos, info.projMat);

    if (texCoord.xy != clamp(texCoord.xy, 0.0, 1.0)) return 0.0;

    float threshold = LinearDepth(info.shadowMapPos, info.clip);
    vec2 moments = texture(TEXTURE_SAMPLER(texCoord.xy * info.mapOffset.zw + info.mapOffset.xy)).xy;

    float p = step(threshold, moments.x);
    float variance = max(moments.y - moments.x * moments.x, varianceMin);

    // Apply Chebyshev's inequality
    float d = threshold - moments.x;
    float pMax = smoothstep(lightBleedReduction, 1.0, variance / (variance + d * d));

    return min(max(p, pMax), 1.0);
}
