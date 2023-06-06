/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#version 460
#extension GL_EXT_shader_16bit_storage : require

#include "../lib/vertex_base.glsl"
#include "lib/scene.glsl"
#include "lib/warp_geometry_params.glsl"

struct JointVertex {
    vec4 weights;
    u16vec4 indexes;
};

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(std430, set = 0, binding = 0) readonly buffer DrawParamsList {
    WarpGeometryParams paramList[];
};
layout(std430, set = 0, binding = 1) buffer VertexBufferOutput {
    SceneVertex vertices[];
};
layout(std140, set = 0, binding = 2) uniform JointPoses {
    mat4 jointPoses[100];
};
layout(std430, set = 0, binding = 3) readonly buffer JointVertexData {
    JointVertex jointsData[];
};

void main() {
    WarpGeometryParams params = paramList[gl_BaseInstance];

    mat4 modelMat;

    if (params.jointsVertexOffset == 0xffffffff) {
        modelMat = params.modelMat;
    } else {
        uint jointsIndex = params.jointsVertexOffset + gl_VertexIndex - gl_BaseVertex;
        uvec4 jointIndexes = uvec4(jointsData[jointsIndex].indexes);
        vec4 jointWeights = jointsData[jointsIndex].weights;

        // weights may not sum to 1
        jointWeights /= jointWeights.x + jointWeights.y + jointWeights.z + jointWeights.w;

        modelMat = jointWeights.x * jointPoses[params.jointPosesOffset + jointIndexes.x] +
                   jointWeights.y * jointPoses[params.jointPosesOffset + jointIndexes.y] +
                   jointWeights.z * jointPoses[params.jointPosesOffset + jointIndexes.z] +
                   jointWeights.w * jointPoses[params.jointPosesOffset + jointIndexes.w];
    }

    vec4 position = modelMat * vec4(inPosition, 1);
    vec3 normal = mat3(modelMat) * inNormal;

    // warping goes here
    // float stationRadius = 150;
    // position.y += stationRadius - sqrt(stationRadius * stationRadius - position.x * position.x);

    SceneVertex vertex;
    vertex.positionAndNormalX = vec4(position.xyz, normal.x);
    vertex.normalYZ = normal.yz;
    vertex.uv = inTexCoord;
    vertices[gl_VertexIndex - gl_BaseVertex + params.outputVertexOffset] = vertex;
}
