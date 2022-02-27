#version 450

#extension GL_OVR_multiview2 : enable
layout(num_views = 2) in;

#define SAMPLER_TYPE sampler2DArray
#define SAMPLE_COORD_TYPE vec3
#define SAMPLE_COORD(coord) vec3(coord, gl_ViewID_OVR)
#define SIZE_LARGE

#include "gaussian_blur.frag.glsl"
