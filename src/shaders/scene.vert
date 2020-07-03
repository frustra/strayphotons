#version 430

##import lib/vertex_base
##import lib/skeleton_animate

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexCoord;
layout (location = 3) in vec4 inWeights;
layout (location = 4) in ivec4 inJoints;

uniform mat4 model;
uniform mat4 primitive;
uniform mat4 view;
uniform mat4 projection;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outTexCoord;

void main()
{
	//gl_Position = projection * view * model * vec4(inPos, 1.0);
	//mat4 normalMat = view * model;
	//outViewPos = vec3(normalMat * vec4(inPos, 1.0));
	//outNormal = mat3(normalMat) * inNormal;
	//outTexCoord = inTexCoord;
	
	mat4 poseMat = model * primitive * getSkinMatrix(inWeights, inJoints);

	gl_Position = poseMat * vec4(inPos, 1.0);
	outNormal = mat3(poseMat) * inNormal;
	outTexCoord = inTexCoord;
}
