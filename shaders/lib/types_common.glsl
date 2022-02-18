#ifndef TYPES_COMMON_GLSL_INCLUDED
#define TYPES_COMMON_GLSL_INCLUDED

#ifndef MAX_LIGHTS
    #define MAX_LIGHTS 16
#endif

#ifndef MAX_LIGHT_GELS
    #define MAX_LIGHT_GELS 2
#endif

const float punctualLightSizeSq = 0.01 * 0.01; // 1cm punctual lights

struct ViewState {
    mat4 projMat, invProjMat;
    mat4 viewMat, invViewMat;
    vec2 clip;
    vec2 extents;
};

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

    int gelId;
};

struct LightSensor {
    vec3 position;
    float id0;
    vec3 direction;
    float id1;
};

struct VoxelArea {
    vec3 areaMin;
    vec3 areaMax;
};

#ifdef MAX_VOXEL_AREAS
struct VoxelInfo {
    vec3 center;
    float size;
    VoxelArea areas[MAX_VOXEL_AREAS];
};
#endif

#endif
