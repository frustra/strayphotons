#version 450

#extension GL_OVR_multiview2 : enable
layout(num_views = 2) in;

layout (binding = 0) uniform sampler2DArray colorTex;
layout (binding = 1) uniform sampler2DArray weightTex;

#include "common.glsl"

layout (location = 0) in vec2 inTexCoord;
layout (location = 0) out vec4 outFragColor;

void main()
{
	vec4 offset;
	offset.x = textureSize(colorTex, 0).x;
	offset.y = textureSize(weightTex, 0).x;
	SMAANeighborhoodBlendingVS(inTexCoord, offset);
	outFragColor = SMAANeighborhoodBlendingPS(inTexCoord, offset, colorTex, weightTex);
}
