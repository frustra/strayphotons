#version 430

##import lib/vertex_base

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexCoord;

uniform mat4 projMatrix;
uniform mat4 modelMatrix;
uniform mat4 viewMatrix;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outTexCoord;

void main()
{
	gl_Position = projMatrix * viewMatrix * modelMatrix * vec4(inPos, 1.0);
	outNormal = mat3(viewMatrix * modelMatrix) * inNormal;
	outTexCoord = inTexCoord;
}
