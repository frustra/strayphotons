##import lib/mirror_common

#define SCENE_MIRROR_LIST_SIZE MAX_MIRRORS * MAX_MIRROR_RECURSION

layout(binding = 1, std430) buffer MirrorSceneData {
	int count[4]; // array instead of multiple elements due to an nvidia driver bug with SSBOs in geometry shaders (C5133)
	uint mask[SCENE_MIRROR_LIST_SIZE];
	uint list[SCENE_MIRROR_LIST_SIZE]; // 1 bit source flag, 15 bits source index, 1 bit empty, 15 bits target mirror ID
	mat4 reflectMat[SCENE_MIRROR_LIST_SIZE];
	mat4 invReflectMat[SCENE_MIRROR_LIST_SIZE];
	vec4 clipPlane[SCENE_MIRROR_LIST_SIZE];
} mirrorSData;