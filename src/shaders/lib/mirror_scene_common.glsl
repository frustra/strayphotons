##import lib/mirror_common

#define SCENE_MIRROR_DEPTH 2
#define SCENE_MIRROR_LIST_SIZE MAX_MIRRORS * SCENE_MIRROR_DEPTH

layout(binding = 1, std430) buffer MirrorSceneData {
	int count[4]; // array instead of multiple elements due to an nvidia driver bug with SSBOs in geometry shaders (C5133)
	uint mask[MAX_MIRRORS];
	uint list[SCENE_MIRROR_LIST_SIZE];
	int sourceIndex[SCENE_MIRROR_LIST_SIZE];
	mat4 viewMat[SCENE_MIRROR_LIST_SIZE];
	mat4 invViewMat[SCENE_MIRROR_LIST_SIZE];
	mat4 projMat[SCENE_MIRROR_LIST_SIZE];
	mat4 invProjMat[SCENE_MIRROR_LIST_SIZE];
	vec4 clipPlane[SCENE_MIRROR_LIST_SIZE];
} mirrorSData;