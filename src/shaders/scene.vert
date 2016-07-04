#version 430

##import lib/vertex_base

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexCoord;

uniform mat4 mvpMat;
uniform mat3 normalMat;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outTexCoord;

void main()
{
	gl_Position = mvpMat * vec4(inPos, 1.0);
	outNormal = normalMat * inNormal;
	outTexCoord = inTexCoord;
}
