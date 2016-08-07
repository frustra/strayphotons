#version 430

##import lib/util

uniform int depth;

layout (binding = 0, r32ui) uniform uimage3D voxelColorRG;
layout (binding = 1, r32ui) uniform uimage3D voxelColorBA;

void main()
{
	for (int i = 0; i < depth; i++) {
		imageStore(voxelColorRG, ivec3(gl_FragCoord.xy, i), uvec4(0));
		imageStore(voxelColorBA, ivec3(gl_FragCoord.xy, i), uvec4(0));
	}
}
