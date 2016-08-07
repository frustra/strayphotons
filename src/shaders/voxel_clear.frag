#version 430

##import lib/util

uniform int depth;

layout (location = 0, r32ui) uniform uimage3D voxelGrid;

void main()
{
	for (int i = 0; i < depth; i++) {
		imageStore(voxelGrid, ivec3(gl_FragCoord.xy, i), uvec4(0));
	}
}
