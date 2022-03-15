#version 450

#extension GL_OVR_multiview2 : enable
#extension GL_EXT_nonuniform_qualifier : enable
layout(num_views = 2) in;

#define SHADOWS_ENABLED
#define USE_PCF
#define LIGHTING_GELS

#include "lighting.frag.glsl"
