/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef VOXEL_SHARED_GLSL_INCLUDED
#define VOXEL_SHARED_GLSL_INCLUDED

#extension GL_EXT_shader_16bit_storage : require

#include "indirect_commands.glsl"
#include "util.glsl"

struct VoxelFragment {
    u16vec3 position;
    f16vec3 radiance;
    f16vec3 normal;
};

const uint MipmapWorkGroupSize = 256;

const uint[13] MaxFragListMask = uint[](8191, 4095, 2047, 1023, 511, 255, 127, 63, 31, 15, 7, 3, 1);
const uint[13] FragListWidthBits = uint[](13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1);

const mat4[3] AxisSwapForward = mat4[](mat4(mat3(0, 0, 1, 0, 1, 0, -1, 0, 0)),
    mat4(mat3(1, 0, 0, 0, 0, 1, 0, -1, 0)),
    mat4(mat3(1.0)));

const mat3[3] AxisSwapReverse = mat3[](mat3(0, 0, -1, 0, 1, 0, 1, 0, 0), mat3(1, 0, 0, 0, 0, -1, 0, 1, 0), mat3(1.0));

const uint[6] OppositeAxis = uint[](3, 4, 5, 0, 1, 2);
const uint[6] TangentAxisA = uint[](1, 0, 0, 4, 3, 3);
const uint[6] TangentAxisB = uint[](2, 2, 1, 5, 5, 4);
const vec3[6] AxisDirections = vec3[](vec3(1, 0, 0),
    vec3(0, 1, 0),
    vec3(0, 0, 1),
    vec3(-1, 0, 0),
    vec3(0, -1, 0),
    vec3(0, 0, -1));

int DominantAxis(vec3 normal) {
    vec3 absNormal = abs(normal);
    bvec3 mask = greaterThanEqual(absNormal.xyz, max(absNormal.yzx, absNormal.zxy));
    // If axes are equal, multiple bits could be true, make sure only one is set.
    mask.y = mask.y && !mask.x;
    mask.z = mask.z && !(mask.x || mask.y);
    return int(dot(vec3(mask) * sign(normal), vec3(1, 2, 3)));
}

#endif
