#version 430

##import lib/vertex_base

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexCoord;
layout (location = 3) in vec4 inWeights;
layout (location = 4) in ivec4 inJoints;

uniform mat4 model;
uniform mat4 primitive;
uniform mat4 view;
uniform mat4 projection;

layout (location = 0) out vec2 outTexCoord;

##import lib/skeleton_animate

void main()
{
	gl_Position = view * model * primitive * getSkinMatrix(inWeights, inJoints) * vec4(inPos, 1.0);
	outTexCoord = inTexCoord;
}
