#version 430

##import lib/vertex_base
##import lib/skeleton_animate

layout (location = 0) in vec3 inPos;
// Location 1 = Normal
// Location 2 = TexCoord
layout (location = 3) in vec4 inWeights;
layout (location = 4) in ivec4 inJoints;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

layout (location = 0) out vec3 outViewPos;

void main()
{
	outViewPos = vec3(view * model * getSkinMatrix(inWeights, inJoints) * vec4(inPos, 1.0));
	gl_Position = projection * vec4(outViewPos, 1.0);
}
