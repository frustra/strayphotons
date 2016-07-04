#version 430

##import lib/vertex_base

uniform vec4 smaaRTMetrics;

#define SMAA_INCLUDE_PS 0
##import smaa/common

layout (location = 0) in vec3 position;
layout (location = 2) in vec2 texCoord;
layout (location = 0) out vec2 outTexCoord;
layout (location = 1) out vec4[3] outOffset;

void main()
{
	gl_Position = vec4(position, 1);
	SMAAEdgeDetectionVS(texCoord, outOffset);
	outTexCoord = texCoord;
}