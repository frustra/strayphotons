#version 430

##import lib/util

layout (binding = 0) uniform sampler2D baseColorTex;
layout (binding = 1) uniform sampler2D roughnessTex;

layout (location = 0) in vec3 inViewPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexCoord;

layout (location = 0, r32ui) uniform uimage3D voxelGrid;

void main()
{
	imageAtomicAdd(voxelGrid, ivec3(gl_FragCoord.xy, 0), 1);
}
