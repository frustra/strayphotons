#ifndef VOXEL_TRACE_DEBUG_GLSL_INCLUDED
#define VOXEL_TRACE_DEBUG_GLSL_INCLUDED

float GetVoxelNearest(vec3 position, int level, out vec3 radiance) {
    // uint count = imageLoad(fillCounters, ivec3(position)).r;
    vec4 radianceData = texelFetch(voxelRadiance, ivec3(position) >> level, level);
    vec4 normalData = texelFetch(voxelNormals, ivec3(position) >> level, level);
    // radiance = vec3(length(normalData.xyz / normalData.w));
    radiance = abs(normalData.xyz); // radianceData.rgb;
    return radianceData.a;
}

float TraceVoxelGrid(vec3 rayWorldPos, vec3 rayWorldDir, int level, out vec3 hitRadiance) {
    vec3 rayVoxelPos = (voxelInfo.worldToVoxel * vec4(rayWorldPos, 1.0)).xyz;
    vec3 rayVoxelDir = normalize(mat3(voxelInfo.worldToVoxel) * rayWorldDir);

    ivec3 voxelIndex = ivec3(rayVoxelPos);

    vec3 deltaDist = abs(vec3(1.0) / rayVoxelDir);
    vec3 raySign = sign(rayVoxelDir);
    ivec3 rayStep = ivec3(raySign);

    // Distance to next voxel in each axis
    vec3 sideDist = (raySign * (vec3(voxelIndex) - rayVoxelPos) + (raySign * 0.5) + 0.5) * deltaDist;
    int maxIterations = int(max(voxelInfo.gridSize.x, max(voxelInfo.gridSize.y, voxelInfo.gridSize.z)) * 3);
    bvec3 mask;

    for (int i = 0; i < maxIterations; i++) {
        vec3 radiance;
        float alpha = GetVoxelNearest(rayVoxelPos + rayVoxelDir * 0.001, level, radiance);
        if (alpha > 0) {
            hitRadiance = radiance;
            return alpha;
        }

        // Find axis with minimum distance
        mask = lessThanEqual(sideDist.xyz, min(sideDist.yzx, sideDist.zxy));
        rayVoxelPos += rayVoxelDir * dot(vec3(mask), sideDist);
        voxelIndex += ivec3(mask) * rayStep;
        sideDist = (raySign * (vec3(voxelIndex) - rayVoxelPos) + (raySign * 0.5) + 0.5) * deltaDist;
    }

    hitRadiance = vec3(0);
    return 0;
}

#endif
