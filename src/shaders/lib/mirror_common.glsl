layout(binding = 0, std430) buffer MirrorVisData {
	int count;
	uint mask[MAX_LIGHTS];
	uint list[MAX_LIGHTS * MAX_MIRRORS];
	mat4 viewMat[MAX_LIGHTS * MAX_MIRRORS];
	mat4 projMat[MAX_LIGHTS * MAX_MIRRORS];
} mirrorData;
