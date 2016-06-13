#version 430

uniform vec4 smaaRTMetrics;

#define SMAA_INCLUDE_VS 0
##import smaa/common

layout (binding = 0) uniform sampler2D edgesTex;
layout (binding = 1) uniform sampler2D areaTex;
layout (binding = 2) uniform sampler2D searchTex;

layout (location = 0) in vec2 inTexCoord;
layout (location = 1) in vec2 inPixCoord;
layout (location = 2) in vec4[3] inOffset;
layout (location = 0) out vec4 outFragColor;

void main()
{
	outFragColor = SMAABlendingWeightCalculationPS(inTexCoord, inPixCoord, inOffset, edgesTex, areaTex, searchTex, vec4(0.0));
}