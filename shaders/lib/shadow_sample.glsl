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
float SampleOcclusionMirror(vec2 shadowMapCoord, ShadowInfo info, float testDepth) {
#else
float SampleOcclusion(vec2 shadowMapCoord, ShadowInfo info, float testDepth) {
#endif
    vec2 coord = shadowMapCoord * info.mapOffset.zw + info.mapOffset.xy;
    vec4 values;
    values.x = textureOffset(TEXTURE_SAMPLER(coord), ivec2(-1, 0)).r;
    values.y = textureOffset(TEXTURE_SAMPLER(coord), ivec2(1, 0)).r;
    values.z = textureOffset(TEXTURE_SAMPLER(coord), ivec2(0, -1)).r;
    values.w = textureOffset(TEXTURE_SAMPLER(coord), ivec2(0, 1)).r;

    vec4 deltaDepths = values - testDepth;
    float maxDelta = max(deltaDepths.x, max(deltaDepths.y, max(deltaDepths.z, deltaDepths.w)));
    vec4 samples = step(testDepth - maxDelta, values);
    return smoothstep(0, 4, samples.x + samples.y + samples.z + samples.w);
}

#ifdef MIRROR_SAMPLE
float DirectOcclusionMirror(ShadowInfo info, vec3 surfaceNormal, mat2 rotation0) {
    vec2 mapSize = textureSize(mirrorShadowMap, 0).xy * info.mapOffset.zw;
#else
float DirectOcclusion(ShadowInfo info, vec3 surfaceNormal, mat2 rotation0) {
    vec2 mapSize = textureSize(shadowMap, 0).xy * info.mapOffset.zw;
#endif
    vec2 texelSize = 1.0 / mapSize;
    vec2 shadowMapCoord = ViewPosToScreenPos(info.shadowMapPos, info.projMat).xy;

    // Clip and smooth out the edges of the shadow map so we don't sample neighbors
    if (shadowMapCoord != clamp(shadowMapCoord, 0.0, 1.0)) return 0.0;
    vec2 edgeTerm = linstep(texelSize, texelSize * 2, shadowMapCoord);
    edgeTerm *= linstep(1.0 - texelSize, 1.0 - texelSize * 2, shadowMapCoord);

    // Calculate the the shadow map depth for the current fragment
    vec3 rayDir = normalize(vec3(shadowMapCoord * info.nearInfo.zw + info.nearInfo.xy, -info.clip.x));
    float t = dot(surfaceNormal, info.shadowMapPos) / dot(surfaceNormal, rayDir);
    float fragmentDepth = LinearDepth(rayDir * t, info.clip);

    float shadowBias = shadowBiasDistance / (info.clip.y - info.clip.x);
    float testDepth = fragmentDepth - shadowBias;

    // #ifdef MIRROR_SAMPLE
    //     return edgeTerm.x * edgeTerm.y * SampleOcclusionMirror(shadowMapCoord, info, testDepth);
    // #else
    //     return edgeTerm.x * edgeTerm.y * SampleOcclusion(shadowMapCoord, info, testDepth);
    // #endif

    float occlusion = 0;
    vec2 offset = rotation0 * vec2(texelSize.x, 0);
#ifdef MIRROR_SAMPLE
    occlusion += SampleOcclusionMirror(shadowMapCoord + offset, info, testDepth);
    offset = mat2(0, -1, 1, 0) * offset;
    occlusion += SampleOcclusionMirror(shadowMapCoord + offset, info, testDepth);
    offset = mat2(0, -1, 1, 0) * offset;
    occlusion += SampleOcclusionMirror(shadowMapCoord + offset, info, testDepth);
    offset = mat2(0, -1, 1, 0) * offset;
    occlusion += SampleOcclusionMirror(shadowMapCoord + offset, info, testDepth);
#else
    occlusion += SampleOcclusion(shadowMapCoord + offset, info, testDepth);
    offset = mat2(0, -1, 1, 0) * offset;
    occlusion += SampleOcclusion(shadowMapCoord + offset, info, testDepth);
    offset = mat2(0, -1, 1, 0) * offset;
    occlusion += SampleOcclusion(shadowMapCoord + offset, info, testDepth);
    offset = mat2(0, -1, 1, 0) * offset;
    occlusion += SampleOcclusion(shadowMapCoord + offset, info, testDepth);
#endif

    return edgeTerm.x * edgeTerm.y * linstep(0.05, 1.0, occlusion * 0.25);
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
