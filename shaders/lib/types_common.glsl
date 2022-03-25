#ifndef TYPES_COMMON_GLSL_INCLUDED
#define TYPES_COMMON_GLSL_INCLUDED

#ifndef MAX_LIGHTS
    #define MAX_LIGHTS 64
#endif

#ifndef MAX_OPTICS
    #define MAX_OPTICS 16
#endif

#ifndef MAX_VOXEL_FRAGMENT_LISTS
    #define MAX_VOXEL_FRAGMENT_LISTS 16
#endif

const float punctualLightSizeSq = 0.01 * 0.01; // 1cm punctual lights

struct ViewState {
    mat4 projMat, invProjMat;
    mat4 viewMat, invViewMat;
    vec2 extents, invExtents;
    vec2 clip;
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

struct VoxelState {
    mat4 worldToVoxel;
    ivec3 gridSize;
};

#endif
