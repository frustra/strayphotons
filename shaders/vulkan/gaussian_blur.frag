#version 450

#define SAMPLER_TYPE sampler2D
#define SAMPLE_COORD_TYPE vec2
#define SAMPLE_COORD(coord) coord
#define SIZE_SMALL

#include "gaussian_blur.frag.glsl"
