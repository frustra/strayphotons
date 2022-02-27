#version 450

#extension GL_OVR_multiview2 : enable
layout(num_views = 2) in;

#define SHADOWS_ENABLED
#define USE_PCF
#define LIGHTING_GEL
#define MULTIPLE_LIGHTING_GELS

#include "lighting.frag.glsl"
