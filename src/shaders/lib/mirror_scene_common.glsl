##import lib/mirror_common

#define SCENE_MIRROR_DEPTH 2
#define SCENE_MIRROR_LIST_SIZE MAX_MIRRORS * SCENE_MIRROR_DEPTH

layout(binding = 1, std430) buffer MirrorSceneData {
	int count[4]; // array instead of multiple elements due to an nvidia driver bug with SSBOs in geometry shaders (C5133)
	uint mask[MAX_MIRRORS];
	uint list[SCENE_MIRROR_LIST_SIZE]; // 16 bits source index, 16 bits target mirror ID
	mat4 reflectMat[SCENE_MIRROR_LIST_SIZE];
	vec4 clipPlane[SCENE_MIRROR_LIST_SIZE];
} mirrorSData;