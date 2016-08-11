#version 430

##import lib/util
##import voxel_shared

layout (binding = 1, r32ui) uniform uimage3D voxelPackedColor;
layout (binding = 2, r32ui) uniform uimage3D voxelPackedNormal;
layout (binding = 3, rgba8) uniform image3D voxelColor;
layout (binding = 4, rgba8) uniform image3D voxelNormal;

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inTexCoord;
layout (location = 2) in flat int inDirection;

uniform int level;

in vec4 gl_FragCoord;

void main()
{
	vec3 position = vec3(gl_FragCoord.xy, gl_FragCoord.z * VoxelGridSize);
	position = AxisSwapReverse[abs(inDirection)-1] * (position - VoxelGridSize / 2);
	position += VoxelGridSize / 2;

	vec4 color;
	if (level > 0) {
		if (UnpackVoxel(voxelPackedColor, ivec3(position), color)) {
			imageStore(voxelColor, ivec3(position) >> level, vec4(color.rgb, 1.0));
			imageStore(voxelNormal, ivec3(position) >> level, ReadVoxel(voxelPackedNormal, ivec3(position))/color.a);
		}
	} else if (UnpackVoxelAndClear(voxelPackedColor, ivec3(position), color)) {
		imageStore(voxelColor, ivec3(position), vec4(color.rgb, 1.0));
		imageStore(voxelNormal, ivec3(position), ReadVoxelAndClear(voxelPackedNormal, ivec3(position))/color.a);
	}
}