#ifndef SKELETON_ANIMATE_GLSL_INCLUDED
#define SKELETON_ANIMATE_GLSL_INCLUDED

uniform int boneCount;
layout (binding = 2, std140) uniform BoneData
{
	mat4 bone[255];
} boneData;

mat4 getSkinMatrix(vec4 weights, ivec4 joints)
{
    mat4 skinMat = mat4(1.0f);

	if (boneCount > 0)
	{
		skinMat = 
	    	(float(weights.x) * boneData.bone[joints.x]) + 
			(float(weights.y) * boneData.bone[joints.y]) + 
			(float(weights.z) * boneData.bone[joints.z]) +
			(float(weights.w) * boneData.bone[joints.w]);

		skinMat /= skinMat[3][3];
	}

    return skinMat;
}

#endif