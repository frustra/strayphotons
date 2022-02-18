#version 460
#extension GL_EXT_shader_16bit_storage : require

#include "../lib/vertex_base.glsl"
#include "lib/scene.glsl"
#include "lib/warp_geometry_params.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(std430, set = 0, binding = 0) readonly buffer DrawParamsList {
    WarpGeometryParams paramList[];
};
layout(std430, set = 0, binding = 1) buffer VertexBufferOutput {
    SceneVertex vertices[];
};

void main() {
    WarpGeometryParams params = paramList[gl_BaseInstance];

    vec4 position = params.modelMat * vec4(inPosition, 1);
    vec3 normal = mat3(params.modelMat) * inNormal;

    // warping goes here

    SceneVertex vertex;
    vertex.positionAndNormalX = vec4(position.xyz, normal.x);
    vertex.normalYZ = normal.yz;
    vertex.uv = inTexCoord;
    vertices[gl_VertexIndex - gl_BaseVertex + params.outputVertexOffset] = vertex;
}
