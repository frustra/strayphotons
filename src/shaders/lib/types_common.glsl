#ifndef TYPES_COMMON_GLSL_INCLUDED
#define TYPES_COMMON_GLSL_INCLUDED

#define MAX_LIGHTS 16
#define MAX_MIRRORS 16
#define MAX_LIGHT_SENSORS 32

const float punctualLightSizeSq = 0.01 * 0.01; // 1cm punctual lights

struct Mirror {
	mat4 modelMat;
	mat4 reflectMat;
	vec4 plane;
	vec2 size;
};

struct Light {
	vec3 position;
	float spotAngleCos;

	vec3 tint;
	float intensity;

	vec3 direction;
	float illuminance;

	mat4 proj;
	mat4 invProj;
	mat4 view;
	vec4 mapOffset;
	vec2 clip;
};

struct LightSensor {
	vec3 position; float id0;
	vec3 direction; float id1;
};

#endif
