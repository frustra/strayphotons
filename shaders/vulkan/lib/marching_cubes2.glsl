/*
 * Stray Photons - Copyright (C) 2026 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

// Data translated from reference: https://thomas.lewiner.org/pdfs/marching_cubes_jgt.pdf
const ivec3[8] vertexOffset = ivec3[](ivec3(0, 0, 0),
    ivec3(1, 0, 0),
    ivec3(1, 1, 0),
    ivec3(0, 1, 0),
    ivec3(0, 0, 1),
    ivec3(1, 0, 1),
    ivec3(1, 1, 1),
    ivec3(0, 1, 1));

// ivec4(ivec3(cellOffset), edgeIndex)
const ivec4[13] edgeLookup = ivec4[](ivec4(0, 0, 0, 0),
    ivec4(1, 0, 0, 1),
    ivec4(0, 1, 0, 0),
    ivec4(0, 0, 0, 1),
    ivec4(0, 0, 1, 0),
    ivec4(1, 0, 1, 1),
    ivec4(0, 1, 1, 0),
    ivec4(0, 0, 1, 1),
    ivec4(0, 0, 0, 2),
    ivec4(1, 0, 0, 2),
    ivec4(1, 1, 0, 2),
    ivec4(0, 1, 0, 2),
    ivec4(0, 0, 0, 3));

// Pairs of indexes into vertexOffset representing an edge
// Last index represents center
const uvec2[13] edges = uvec2[](uvec2(0, 1),
    uvec2(1, 2),
    uvec2(2, 3),
    uvec2(0, 3),
    uvec2(4, 5),
    uvec2(5, 6),
    uvec2(6, 7),
    uvec2(3, 7),
    uvec2(0, 4),
    uvec2(1, 5),
    uvec2(2, 6),
    uvec2(3, 7),
    uvec2(0, 6));

// Sets of 4 indexes into vertexOffset representing a face
const uvec4[6] faces = uvec4[](uvec4(0, 4, 5, 1),
    uvec4(1, 5, 6, 2),
    uvec4(2, 6, 7, 3),
    uvec4(3, 7, 4, 0),
    uvec4(0, 3, 2, 1),
    uvec4(4, 7, 6, 5));

#include "../../lib/perlin.glsl"
float sampleGrid(uvec3 gridSize, vec3 position) {
    vec3 cubeVec = (0.5 - 2.0 / gridSize) - abs(vec3(position - gridSize / 2) / gridSize);
    float cube = min(cubeVec.x, min(cubeVec.y, cubeVec.z));
    float sphere = (0.4 - 2.0 / max(gridSize.x, max(gridSize.y, gridSize.z))) -
                   length((vec3(position) - vec3(gridSize) / 2) / gridSize);
    float result = min(cube, sphere - PerlinNoise3D(vec3(position / gridSize) * 3.0 + vec3(seed * 1000, 0, 0)) * 0.1);
    return result;
    // result = clamp(result * 50, -1, 1);
    // float scale = 256;
    // return round(result * scale) / scale;
}
