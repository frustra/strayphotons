#version 430

##import lib/util

uniform int depth;

layout (binding = 0, r32ui) uniform uimage3D voxelColor;

void main()
{
	for (int i = 0; i < depth; i++) {
		imageStore(voxelColor, ivec3(gl_FragCoord.xy, i), uvec4(0));
	}
}
