#version 430

##import lib/vertex_base

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inTexCoord;

uniform mat4 projMatrix;
uniform mat4 modelMatrix;
uniform mat4 viewMatrix;

layout (location = 0) out vec3 outColor;
layout (location = 1) out vec2 outTexCoord;

void main()
{
	outColor = inColor;
	outTexCoord = inTexCoord;
	gl_Position = projMatrix * viewMatrix * modelMatrix * vec4(inPos, 1.0);
}
