#version 430

##import lib/util
##import voxel_shared

layout (binding = 0) uniform sampler2D baseColorTex;
layout (binding = 1) uniform sampler2D roughnessTex;

layout (binding = 0) uniform atomic_uint fragListSize;
layout (binding = 0, offset = 4) uniform atomic_uint nextComputeSize;

layout (binding = 0, r32ui) uniform uimage2D voxelFragList;
layout (binding = 1, r32ui) uniform uimage3D voxelColor;
layout (binding = 2, r32ui) uniform uimage3D voxelNormal;

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inTexCoord;
layout (location = 2) in flat int inDirection;

in vec4 gl_FragCoord;

void main()
{
	vec4 diffuseColor = texture(baseColorTex, inTexCoord);
	if (diffuseColor.a < 0.5) discard;

	float roughness = texture(roughnessTex, inTexCoord).r;

	vec3 position = vec3(gl_FragCoord.xy, gl_FragCoord.z * VoxelGridSize);
	position = AxisSwapReverse[abs(inDirection)-1] * (position - VoxelGridSize / 2);
	position += VoxelGridSize / 2;

	uint rg = (uint(diffuseColor.r * 0xFF) << 16) + uint(diffuseColor.g * 0xFF);
	uint ba = (uint(diffuseColor.b * 0xFF) << 16) + 1;
	imageAtomicAdd(voxelColor, ivec3(floor(position.x) * 2, position.yz), rg);
	uint prevData = imageAtomicAdd(voxelColor, ivec3(floor(position.x) * 2 + 1, position.yz), ba);

	vec3 normal = inNormal * 127.5 + 127.5;
	uint xy = (uint(normal.x) << 16) + uint(normal.y);
	uint zw = (uint(normal.z) << 16) + 1;
	imageAtomicAdd(voxelNormal, ivec3(floor(position.x) * 2, position.yz), xy);
	imageAtomicAdd(voxelNormal, ivec3(floor(position.x) * 2 + 1, position.yz), zw);

	if ((prevData & 0xFFFF) == 0) {
		uint index = atomicCounterIncrement(fragListSize);
		if (index % MipmapWorkGroupSize == 0) atomicCounterIncrement(nextComputeSize);

		uint packedData = (uint(position.x) & 0x3FF) << 20;
		packedData += (uint(position.y) & 0x3FF) << 10;
		packedData += uint(position.z) & 0x3FF;
		imageStore(voxelFragList, ivec2(index & MaxFragListMask, index >> FragListWidthBits), uvec4(packedData));
	}
}
