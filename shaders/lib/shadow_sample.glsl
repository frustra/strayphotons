/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#undef TEXTURE_SAMPLER
#ifdef MIRROR_SAMPLE
    #define TEXTURE_SAMPLER(coord) mirrorShadowMap, vec3(coord, info.index)
#else
    #define TEXTURE_SAMPLER(coord) shadowMap, coord
#endif

struct ShadowInfo {
    vec3 shadowMapPos;
    mat4 projMat;
    vec4 mapOffset;
    vec2 clip;
    vec4 nearInfo;
};

#ifdef MIRROR_SAMPLE
float SimpleOcclusionMirror(ShadowInfo info) {
#else
float SimpleOcclusion(ShadowInfo info) {
#endif
    vec2 mapSize = textureSize(shadowMap, 0).xy * info.mapOffset.zw;
    vec3 texCoord = ViewPosToScreenPos(info.shadowMapPos, info.projMat);
    const vec2 shadowSampleWidth = 0.5 / mapSize;

    if (texCoord.xy != clamp(texCoord.xy, 0.0, 1.0)) return 0.0;
    texCoord.xy = clamp(texCoord.xy, shadowSampleWidth, 1.0 - shadowSampleWidth);

    float shadowBiasMin = shadowBiasDistanceMin / (info.clip.y - info.clip.x);
    float shadowBiasMax = shadowBiasDistanceMax / (info.clip.y - info.clip.x);

    float testDepth = LinearDepth(info.shadowMapPos, info.clip);
    float sampledDepth = texture(TEXTURE_SAMPLER(texCoord.xy * info.mapOffset.zw + info.mapOffset.xy)).r;

    return smoothstep(testDepth - shadowBiasMax, testDepth - shadowBiasMin, sampledDepth);
}

#ifdef MIRROR_SAMPLE
float DirectOcclusionMirror(ShadowInfo info, vec3 surfaceNormal, mat2 rotation0) {
    vec2 mapSize = textureSize(mirrorShadowMap, 0).xy * info.mapOffset.zw;
#else
float DirectOcclusion(ShadowInfo info, vec3 surfaceNormal, mat2 rotation0) {
    vec2 mapSize = textureSize(shadowMap, 0).xy * info.mapOffset.zw;
#endif
    vec2 shadowMapCoord = ViewPosToScreenPos(info.shadowMapPos, info.projMat).xy;
    const vec2 shadowSampleOffset = SHADOW_MAP_SAMPLE_WIDTH / mapSize;
    const vec2 shadowSampleWidth = (SHADOW_MAP_SAMPLE_WIDTH + 0.5) / mapSize;

    // Clip and smooth out the edges of the shadow map so we don't sample neighbors
    if (shadowMapCoord != clamp(shadowMapCoord, 0.0, 1.0)) return 0.0;
    vec2 edgeTerm = linstep(vec2(0.0), shadowSampleWidth, shadowMapCoord);
    edgeTerm *= linstep(vec2(1.0), 1.0 - shadowSampleWidth, shadowMapCoord);
    shadowMapCoord = clamp(shadowMapCoord, shadowSampleWidth, 1.0 - shadowSampleWidth);

    // Calculate the the shadow map depth for the current fragment
    vec3 rayDir = normalize(vec3(shadowMapCoord * info.nearInfo.zw + info.nearInfo.xy, -info.clip.x));
    float t = dot(surfaceNormal, info.shadowMapPos) / dot(surfaceNormal, rayDir);
    float fragmentDepth = LinearDepth(rayDir * t, info.clip);
    vec2 sampleScale = vec2(1.0);// - cross(surfaceNormal, rayDir).yx * 0.75;

    float values[SHADOW_MAP_SAMPLE_COUNT];
    float theta = M_PI / 2.0; // 45 degree start
    float r = 1.0;
    float avgDepth = 0;
    for (int i = 0; i < SHADOW_MAP_SAMPLE_COUNT; i++) {
        vec2 spiralXY = vec2(sin(theta), cos(theta)) * sqrt(r);
        values[i] = texture(TEXTURE_SAMPLER((shadowMapCoord + rotation0 * spiralXY * shadowSampleOffset * sampleScale) * info.mapOffset.zw + info.mapOffset.xy)).r;
        avgDepth += values[i] * (1.0 - r);
        theta += 2.0 * M_PI / M_GOLDEN_RATIO;
        r -= 1.0 / SHADOW_MAP_SAMPLE_COUNT;
    }
    avgDepth /= (SHADOW_MAP_SAMPLE_COUNT+1) * 0.5;

    float shadowBiasMin = shadowBiasDistanceMin / (info.clip.y - info.clip.x);
    float shadowBiasMax = shadowBiasDistanceMax / (info.clip.y - info.clip.x);

    float totalSample = 0.0;
    for (int i = 0; i < SHADOW_MAP_SAMPLE_COUNT; i++) {
        float lowerBlend = fragmentDepth - max(0, avgDepth - values[i]) - shadowBiasMax;
        float upperBlend = fragmentDepth - max(0, avgDepth - values[i]) - shadowBiasMin;
        totalSample += smoothstep(lowerBlend, upperBlend, values[i]);
    }
    float weight = length(cross(surfaceNormal, rayDir)) * 0.5; // = sin(angle of incidence) / 2
    return edgeTerm.x * edgeTerm.y * smoothstep(0.125 * (1 - weight), 0.875 - weight, totalSample / SHADOW_MAP_SAMPLE_COUNT);
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
