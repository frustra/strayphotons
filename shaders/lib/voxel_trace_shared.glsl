#ifndef VOXEL_TRACE_SHARED_GLSL_INCLUDED
#define VOXEL_TRACE_SHARED_GLSL_INCLUDED

#include "spatial_util.glsl"

vec4 ConeTraceGrid(float ratio, vec3 rayPos, vec3 rayDir, vec3 surfaceNormal, vec2 fragCoord) {
    vec3 voxelPos = (voxelInfo.worldToVoxel * vec4(rayPos, 1.0)).xyz;
    vec3 voxelDir = normalize(mat3(voxelInfo.worldToVoxel) * rayDir);

    float dist = InterleavedGradientNoise(fragCoord);
    vec4 result = vec4(0);

    // TODO: Fix this constant
    for (int i = 0; i < 200; i++) {
        float size = max(1.0, ratio * dist);
        vec3 position = voxelPos + voxelDir * dist;
        position += size * surfaceNormal;

        float level = max(0, log2(size));
        vec4 value = textureLod(voxelRadiance, position / voxelInfo.gridSize, level);
        vec4 normal = textureLod(voxelNormals, position / voxelInfo.gridSize, level);
        normal.xyz /= max(0.001, normal.w);
        value.a *= step(0.001, normal.w);

        // Bias the alpha to prevent traces going through objects.
        value.a = smoothstep(0.0, 0.35, value.a);
        value.a *= max(step(0, dot(rayDir, -normal.xyz)), 1 - smoothstep(0.2, 0.4, length(normal.xyz)));
        result += vec4(value.rgb, value.a) * (1.0 - result.a) * (1 - step(0, -value.a));

        if (result.a > 0.999) break;
        dist += size;
    }

    return result;
}

vec4 ConeTraceGridDiffuse(vec3 rayPos, vec3 rayDir, vec3 surfaceNormal, float offset) {
    vec3 voxelPos = (voxelInfo.worldToVoxel * vec4(rayPos, 1.0)).xyz;
    vec3 voxelDir = normalize(mat3(voxelInfo.worldToVoxel) * rayDir);

    float dist = saturate(offset);
    float size = 1.0;

    vec4 result = vec4(0);

    // TODO: Fix this constant
    for (int level = 0; level < 8; level++) {
        vec3 position = voxelPos + voxelDir * dist;
        vec4 value = textureLod(voxelRadiance, position / voxelInfo.gridSize, level);
        vec4 normal = textureLod(voxelNormals, position / voxelInfo.gridSize, level);
        normal.xyz /= max(0.001, normal.w);
        value.a *= step(0.001, normal.w);

        value.rgb *= smoothstep(0.2, 0.4, length(normal.xyz));
        value.a *= smoothstep(-0.1, 0.0, dot(rayDir, -normal.xyz));

        result += vec4(value.rgb, value.a) * (1.0 - result.a) * (1 - step(0, -value.a));

        size *= 2.0;
        dist += size;
    }

    return result;
}

vec3 HemisphereIndirectDiffuse(vec3 worldPosition, vec3 worldNormal, vec2 fragCoord) {
    float rOffset = InterleavedGradientNoise(fragCoord);

    vec3 samplePos = worldPosition;
    vec3 sampleDir = OrientByNormal(rOffset * M_PI * 2.0, 0.1, worldNormal);
    vec4 indirectDiffuse = ConeTraceGridDiffuse(samplePos, sampleDir, worldNormal, rOffset);

    for (int a = 3; a <= 6; a += 3) {
        float diffuseScale = 1.0 / a;
        for (float r = 0; r < a; r++) {
            vec3 sampleDir = OrientByNormal((r + rOffset) * diffuseScale * M_PI * 2.0, a * 0.1, worldNormal);
            float offset = InterleavedGradientNoise(vec2(rOffset, fragCoord.y));
            vec4 sampleColor = ConeTraceGridDiffuse(samplePos, sampleDir, worldNormal, offset);

            indirectDiffuse += sampleColor * dot(sampleDir, worldNormal) * diffuseScale;
        }
    }

    return indirectDiffuse.rgb * 0.333;
}

#endif
