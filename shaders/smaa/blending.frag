#version 430

#define SMAA_INCLUDE_VS 0
#include "common.glsl"

layout (binding = 0) uniform sampler2D colorTex;
layout (binding = 1) uniform sampler2D weightTex;

layout (location = 0) in vec2 inTexCoord;
layout (location = 1) in vec4 inOffset;
layout (location = 0) out vec4 outFragColor;

void main()
{
	outFragColor = SMAANeighborhoodBlendingPS(inTexCoord, inOffset, colorTex, weightTex);
}
