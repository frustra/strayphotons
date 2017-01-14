#version 430

#define DIFFUSE_ONLY_SHADING

##import lib/util
##import voxel_shared
##import lib/lighting_util

layout (binding = 0) uniform sampler2D baseColorTex;
layout (binding = 1) uniform sampler2D roughnessTex;
// binding 2 = metallicTex
// binding 3 = heightTex
layout (binding = 4) uniform sampler2D shadowMap;

layout (binding = 0) uniform atomic_uint fragListSize;
layout (binding = 0, offset = 4) uniform atomic_uint nextComputeSize;

layout (binding = 0, r32ui) writeonly uniform uimage2D voxelFragList;
layout (binding = 1, r32ui) uniform uimage3D voxelData;

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inTexCoord;
layout (location = 2) in flat int inDirection;

##import lib/light_inputs

uniform float voxelSize = 0.1;
uniform vec3 voxelGridCenter = vec3(0);

in vec4 gl_FragCoord;

##import lib/shadow_sample

// Data format: [color.r, color.g], [color.b, radiance.r], [radiance.g, radiance.b], [normal.x, normal.y], [normal.z, roughness], [count]

void main()
{
	vec4 baseColor = texture(baseColorTex, inTexCoord);
	if (baseColor.a < 0.5) discard;

	float roughness = texture(roughnessTex, inTexCoord).r;

	vec3 position = vec3(gl_FragCoord.xy / 2.0, gl_FragCoord.z * VoxelGridSize);
	position = AxisSwapReverse[abs(inDirection)-1] * (position - VoxelGridSize / 2);
	vec3 worldPosition = position * voxelSize + voxelGridCenter;
	position += VoxelGridSize / 2;

	vec3 pixelLuminance = directShading(worldPosition, baseColor.rgb, inNormal, roughness);

	// Clip so we don't overflow
	pixelLuminance = min(vec3(1), pixelLuminance);

	vec3 normal = normalize(inNormal) * 0.5 + 0.5;

	uint rg = (uint(baseColor.r * 0xFF) << 16) + uint(baseColor.g * 0xFF);
	uint br = (uint(baseColor.b * 0xFF) << 16) + uint(pixelLuminance.r * 0x7FF);
	uint gb = (uint(pixelLuminance.g * 0x7FF) << 16) + uint(pixelLuminance.b * 0x7FF);
	uint xy = (uint(normal.x * 0x7FF) << 16) + uint(normal.y * 0x7FF);
	uint zr = (uint(normal.z * 0x7FF) << 16) + uint(roughness * 0xFF);
	imageAtomicAdd(voxelData, ivec3(floor(position.x) * 6, position.yz), rg);
	imageAtomicAdd(voxelData, ivec3(floor(position.x) * 6 + 1, position.yz), br);
	imageAtomicAdd(voxelData, ivec3(floor(position.x) * 6 + 2, position.yz), gb);
	imageAtomicAdd(voxelData, ivec3(floor(position.x) * 6 + 3, position.yz), xy);
	imageAtomicAdd(voxelData, ivec3(floor(position.x) * 6 + 4, position.yz), zr);
	uint prevData = imageAtomicAdd(voxelData, ivec3(floor(position.x) * 6 + 5, position.yz), 1);

	if ((prevData & 0xFFFF) == 0) {
		uint index = atomicCounterIncrement(fragListSize);
		if (index % MipmapWorkGroupSize == 0) atomicCounterIncrement(nextComputeSize);

		uint packedData = (uint(position.x) & 0x3FF) << 20;
		packedData += (uint(position.y) & 0x3FF) << 10;
		packedData += uint(position.z) & 0x3FF;
		imageStore(voxelFragList, ivec2(index & MaxFragListMask[0], index >> FragListWidthBits[0]), uvec4(packedData));
	}
}
