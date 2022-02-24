#version 450

#extension GL_OVR_multiview2 : enable
layout(num_views = 2) in;

layout (binding = 0) uniform sampler2DArray edgesTex;
layout (binding = 1) uniform sampler2D areaTex;
layout (binding = 2) uniform sampler2D searchTex;

#include "common.glsl"

layout (location = 0) in vec2 inTexCoord;
layout (location = 0) out vec4 outFragColor;

void main()
{
	vec2 pixCoord;
	vec4[3] offset;
	offset[0].x = textureSize(edgesTex, 0).x;
	offset[0].y = textureSize(areaTex, 0).x;
	offset[0].z = textureSize(searchTex, 0).x;
	SMAABlendingWeightCalculationVS(inTexCoord, pixCoord, offset);
	outFragColor = SMAABlendingWeightCalculationPS(inTexCoord, pixCoord, offset, edgesTex, areaTex, searchTex, vec4(0.0));
}
