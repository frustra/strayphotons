/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef VOXEL_TRACE_SHARED_GLSL_INCLUDED
#define VOXEL_TRACE_SHARED_GLSL_INCLUDED

#include "spatial_util.glsl"
#include "voxel_shared.glsl"

vec4 ConeTraceGrid(float ratio, vec3 rayPos, vec3 rayDir, vec3 surfaceNormal, vec2 fragCoord) {
    vec3 voxelPos = (voxelInfo.worldToVoxel * vec4(rayPos, 1.0)).xyz;
    vec3 voxelDir = normalize(mat3(voxelInfo.worldToVoxel) * rayDir);

    float dist = InterleavedGradientNoise(fragCoord);
    vec4 result = vec4(0);

    // TODO: Fix this constant
    for (int i = 0; i < 200; i++) {
        float size = max(1.0, ratio * dist);
        vec3 position = voxelPos + voxelDir * dist;
        position += surfaceNormal * length(vec3(1));

        uint layerLevel = clamp(uint(size - 1), 0, VOXEL_LAYERS - 1);
        // TODO: layers > 0 start to blur extremely quickly. An intermediate layer is needed for specular to look good.

        vec4 value = vec4(0);
        for (int axis = 0; axis < 3; axis++) {
            float cosWeight = dot(AxisDirections[axis], voxelDir);
            float axisSign = sign(cosWeight);
            int axisIndex = axis + 3 * (1 - int(step(0, axisSign)));
            vec4 sampleValue = texture(voxelLayersIn[layerLevel * 6 + axisIndex], position / voxelInfo.gridSize);
            sampleValue.a /= max(1, sampleValue.a);
            value += sampleValue * smoothstep(0, 0.4, abs(cosWeight));
        }
        value.rgb /= max(1, value.a);

        result += vec4(value.rgb, value.a) * (1.0 - result.a) * (1 - step(0, -value.a));

        if (result.a > 0.999) break;
        dist += size;
    }

    return result;
}

#endif
