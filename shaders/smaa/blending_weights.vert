#version 430

#include "../lib/vertex_base.glsl"

#define SMAA_INCLUDE_PS 0
#include "common.glsl"

layout (location = 0) in vec3 position;
layout (location = 2) in vec2 texCoord;
layout (location = 0) out vec2 outTexCoord;
layout (location = 1) out vec2 outPixCoord;
layout (location = 2) out vec4[3] outOffset;

void main()
{
	gl_Position = vec4(position, 1);
	SMAABlendingWeightCalculationVS(texCoord, outPixCoord, outOffset);
	outTexCoord = texCoord;
}
