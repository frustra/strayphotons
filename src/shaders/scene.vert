#version 430

##import lib/vertex_base

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexCoord;
layout (location = 3) in vec4 inWeights;  // Weights for Bones that affect this vertex (up to 4)
layout (location = 4) in ivec4 inJoints;

uniform mat4 model;
uniform mat4 primitive;
uniform mat4 view;
uniform mat4 projection;
uniform int boneCount;

layout (binding = 0, std140) uniform BoneData
{
	mat4 bone[255];
} boneData;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outTexCoord;

void main()
{
	//gl_Position = projection * view * model * vec4(inPos, 1.0);
	//mat4 normalMat = view * model;
	//outViewPos = vec3(normalMat * vec4(inPos, 1.0));
	//outNormal = mat3(normalMat) * inNormal;
	//outTexCoord = inTexCoord;

	mat4 skinMat = mat4(1.0f);

	if (boneCount > 0)
	{
		mat4 bone0 = boneData.bone[int(inJoints.x)];
		mat4 bone1 = boneData.bone[int(inJoints.y)];
		mat4 bone2 = boneData.bone[int(inJoints.z)];
		mat4 bone3 = boneData.bone[int(inJoints.w)];

		skinMat = 
	    	(float(inWeights.x) * bone0) + 
			(float(inWeights.y) * bone1) + 
			(float(inWeights.z) * bone2) +
			(float(inWeights.w) * bone3);

		skinMat /= skinMat[3][3];
	}
	
	mat4 poseMat = model * primitive * skinMat;

	gl_Position = poseMat * vec4(inPos, 1.0);
	outNormal = mat3(poseMat) * inNormal;
	outTexCoord = inTexCoord;
}
