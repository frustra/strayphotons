##import lib/mirror_common

layout(binding = 0, std430) buffer MirrorVisData {
	int count[4]; // array instead of multiple elements due to an nvidia driver bug with SSBOs in geometry shaders (C5133)
	uint mask[MAX_LIGHTS * MAX_MIRRORS][MAX_MIRRORS];
	uint list[MAX_LIGHTS * MAX_MIRRORS];
	int sourceLight[MAX_LIGHTS * MAX_MIRRORS];
	mat4 viewMat[MAX_LIGHTS * MAX_MIRRORS];
	mat4 invViewMat[MAX_LIGHTS * MAX_MIRRORS];
	mat4 projMat[MAX_LIGHTS * MAX_MIRRORS];
	mat4 invProjMat[MAX_LIGHTS * MAX_MIRRORS];
	vec2 clip[MAX_LIGHTS * MAX_MIRRORS];
	vec4 nearInfo[MAX_LIGHTS * MAX_MIRRORS];
	vec3 lightDirection[MAX_LIGHTS * MAX_MIRRORS]; // stride of vec4
} mirrorData;