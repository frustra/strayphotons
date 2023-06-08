/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef VOXEL_TRACE_DEBUG_GLSL_INCLUDED
#define VOXEL_TRACE_DEBUG_GLSL_INCLUDED

#extension GL_EXT_nonuniform_qualifier : enable

float GetVoxelNearest(vec3 position, vec3 dir, int layer, out vec3 radiance) {
    int axis = DominantAxis(dir);
    if (axis < 0) {
        axis = -axis + 2;
    } else {
        axis -= 1;
    }
    vec4 voxelData = texelFetch(voxelLayersIn[layer * 6 + axis], ivec3(position), 0);
    radiance = voxelData.rgb;
    return voxelData.a;
}

float TraceVoxelGrid(vec3 rayWorldPos, vec3 rayWorldDir, int layer, out vec3 hitRadiance) {
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

    vec3 lastVoxelDir = rayVoxelDir;
    for (int i = 0; i < maxIterations; i++) {
        vec3 radiance;
        float alpha = GetVoxelNearest(rayVoxelPos + rayVoxelDir * 0.001, lastVoxelDir, layer, radiance);
        if (alpha > 0.001) {
            hitRadiance = radiance;
            return alpha;
        }

        // Find axis with minimum distance
        mask = lessThanEqual(sideDist.xyz, min(sideDist.yzx, sideDist.zxy));
        rayVoxelPos += rayVoxelDir * dot(vec3(mask), sideDist);
        voxelIndex += ivec3(mask) * rayStep;
        lastVoxelDir = vec3(mask) * rayStep;
        sideDist = (raySign * (vec3(voxelIndex) - rayVoxelPos) + (raySign * 0.5) + 0.5) * deltaDist;
    }

    hitRadiance = vec3(0);
    return 0;
}

#endif
