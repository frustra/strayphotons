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
	mat3 modelRotation = mat3(params.modelMat);

	SceneVertex vertex;
	vertex.positionAndNormalX.xyz = inPosition;
	vertex.positionAndNormalX.w = inNormal.x;
	vertex.normalYZ = inNormal.yz;
	vertex.uv = inTexCoord;
	vertices[gl_VertexIndex - gl_BaseVertex + params.outputVertexOffset] = vertex;
}
