#version 430

#define SMAA_INCLUDE_VS 0
#include "common.glsl"

layout (binding = 0) uniform sampler2D gammaCorrLumaTex;

layout (location = 0) in vec2 inTexCoord;
layout (location = 1) in vec4[3] inOffset;
layout (location = 0) out vec4 outFragColor;

void main()
{
	vec2 edgeData = SMAALumaEdgeDetectionPS(inTexCoord, inOffset, gammaCorrLumaTex);
	outFragColor = vec4(edgeData, 0, 1);
}
