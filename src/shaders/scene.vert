#version 430

##import lib/vertex_base

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexCoord;
layout (location = 3) in vec4 inWeights;  // Weights for Bones that affect this vertex (up to 4)
layout (location = 4) in vec4 inJoints;

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
layout (location = 6) out vec3 outRgb;

void main()
{
	//gl_Position = projection * view * model * vec4(inPos, 1.0);
	//mat4 normalMat = view * model;
	//outViewPos = vec3(normalMat * vec4(inPos, 1.0));
	//outNormal = mat3(normalMat) * inNormal;
	//outTexCoord = inTexCoord;

	vec4 vertPose = vec4(inPos, 1.0);
	vec4 newNormal = vec4(inNormal, 0.0);

	outRgb = vec3(0.7, 0.7, 0.7);

	mat4 skinMat = mat4(1.0f);

	if (boneCount > 0)
	{
		vec4 normWeights = normalize(inWeights);

		skinMat = 
	    	normWeights.x * boneData.bone[int(inJoints.x)] + 
			normWeights.y * boneData.bone[int(inJoints.y)] +
			normWeights.z * boneData.bone[int(inJoints.z)] +
			normWeights.w * boneData.bone[int(inJoints.w)];
	}
	
	mat4 poseMat = model * primitive;

	gl_Position = skinMat * vertPose;
	gl_Position.xyz /= gl_Position.w;

	gl_Position = poseMat * gl_Position;

	outNormal = mat3(poseMat) * mat3(skinMat) * inNormal;
	outTexCoord = inTexCoord;
}
