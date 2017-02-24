layout(binding = 0, std430) buffer MirrorVisData {
	int count[4]; // array instead of multiple elements due to an nvidia driver bug with SSBOs in geometry shaders (C5133)
	uint maskL[MAX_LIGHTS];
	uint maskM[MAX_MIRRORS]; // TODO(jli): can use one mask array of size max(MAX_LIGHTS, MAX_MIRRORS)
	uint list[MAX_LIGHTS * MAX_MIRRORS];
	int sourceLight[MAX_LIGHTS * MAX_MIRRORS];
	int sourceIndex[MAX_LIGHTS * MAX_MIRRORS];
	mat4 viewMat[MAX_LIGHTS * MAX_MIRRORS];
	mat4 invViewMat[MAX_LIGHTS * MAX_MIRRORS];
	mat4 projMat[MAX_LIGHTS * MAX_MIRRORS];
	mat4 invProjMat[MAX_LIGHTS * MAX_MIRRORS];
	vec2 clip[MAX_LIGHTS * MAX_MIRRORS];
	vec4 nearInfo[MAX_LIGHTS * MAX_MIRRORS];
	vec3 lightDirection[MAX_LIGHTS * MAX_MIRRORS]; // stride of vec4
} mirrorData;

#define MIRROR_SOURCE_BIT 0x80000000

uint PackLightAndMirror(int light, int mirror) {
	return (uint(light) << 16) + uint(mirror);
}

uint PackMirrorAndMirror(int mirror1, int mirror2) {
	return PackLightAndMirror(mirror1, mirror2) | MIRROR_SOURCE_BIT;
}

int UnpackMirrorSource(uint tuple) {
	return int((tuple >> 16) & 0x7FFF);
}

int UnpackMirrorDest(uint tuple) {
	return int(tuple & 0x7FFF);
}

bool MirrorSourceIsMirror(uint tuple) {
	return (tuple & MIRROR_SOURCE_BIT) == MIRROR_SOURCE_BIT;
}