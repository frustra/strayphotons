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

#define SHADOW_MAP_SAMPLE_WIDTH 3

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

    float shadowBias = shadowBiasDistance / (info.clip.y - info.clip.x);

    float testDepth = LinearDepth(info.shadowMapPos, info.clip);
    float sampledDepth = texture(TEXTURE_SAMPLER(texCoord.xy * info.mapOffset.zw + info.mapOffset.xy)).r;

    return smoothstep(testDepth - shadowBias, testDepth - shadowBias * 0.2, sampledDepth);
}

#ifdef MIRROR_SAMPLE
float DirectOcclusionMirror(ShadowInfo info, vec3 surfaceNormal, mat2 rotation0) {
    vec2 mapSize = textureSize(mirrorShadowMap, 0).xy * info.mapOffset.zw;
#else
float DirectOcclusion(ShadowInfo info, vec3 surfaceNormal, mat2 rotation0) {
    vec2 mapSize = textureSize(shadowMap, 0).xy * info.mapOffset.zw;
#endif
    vec2 shadowMapCoord = ViewPosToScreenPos(info.shadowMapPos, info.projMat).xy;
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

    float values[8] = {
        // clang-format off
        texture(TEXTURE_SAMPLER((shadowMapCoord + rotation0 * SpiralOffsets[0] * shadowSampleWidth) * info.mapOffset.zw + info.mapOffset.xy)).r,
        texture(TEXTURE_SAMPLER((shadowMapCoord + rotation0 * SpiralOffsets[1] * shadowSampleWidth) * info.mapOffset.zw + info.mapOffset.xy)).r,
        texture(TEXTURE_SAMPLER((shadowMapCoord + rotation0 * SpiralOffsets[2] * shadowSampleWidth) * info.mapOffset.zw + info.mapOffset.xy)).r,
        texture(TEXTURE_SAMPLER((shadowMapCoord + rotation0 * SpiralOffsets[3] * shadowSampleWidth) * info.mapOffset.zw + info.mapOffset.xy)).r,
        texture(TEXTURE_SAMPLER((shadowMapCoord + rotation0 * SpiralOffsets[4] * shadowSampleWidth) * info.mapOffset.zw + info.mapOffset.xy)).r,
        texture(TEXTURE_SAMPLER((shadowMapCoord + rotation0 * SpiralOffsets[5] * shadowSampleWidth) * info.mapOffset.zw + info.mapOffset.xy)).r,
        texture(TEXTURE_SAMPLER((shadowMapCoord + rotation0 * SpiralOffsets[6] * shadowSampleWidth) * info.mapOffset.zw + info.mapOffset.xy)).r,
        texture(TEXTURE_SAMPLER((shadowMapCoord + rotation0 * SpiralOffsets[7] * shadowSampleWidth) * info.mapOffset.zw + info.mapOffset.xy)).r
        // clang-format on
    };

    float avgDepth = (values[0] + values[1] + values[2] + values[3] + values[4] + values[5] + values[6] + values[7]) *
                     0.125;
    float shadowBias = shadowBiasDistance / (info.clip.y - info.clip.x);
    float testDepth = fragmentDepth - shadowBias;

    float totalSample = step(testDepth - max(0, avgDepth - values[0]), values[0]) +
                        step(testDepth - max(0, avgDepth - values[1]), values[1]) +
                        step(testDepth - max(0, avgDepth - values[2]), values[2]) +
                        step(testDepth - max(0, avgDepth - values[3]), values[3]) +
                        step(testDepth - max(0, avgDepth - values[4]), values[4]) +
                        step(testDepth - max(0, avgDepth - values[5]), values[5]) +
                        step(testDepth - max(0, avgDepth - values[6]), values[6]) +
                        step(testDepth - max(0, avgDepth - values[7]), values[7]);

    return edgeTerm.x * edgeTerm.y * smoothstep(1, 7, totalSample);
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
