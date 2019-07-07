#version 430

#define DIFFUSE_ONLY_SHADING
#define INCLUDE_MIRRORS
#define LIGHTING_GEL

##import lib/util
##import voxel_shared

##import lib/types_common

layout (binding = 0) uniform sampler2D baseColorTex;
// binding 1 = roughnessTex
layout (binding = 2) uniform sampler2D metallicTex;
// binding 3 = heightTex
layout (binding = 4) uniform sampler2D shadowMap;
layout (binding = 5) uniform sampler2DArray mirrorShadowMap;
layout (binding = 6) uniform sampler2D lightingGel;
layout (binding = 7) uniform sampler3D voxelRadiance;
layout (binding = 8) uniform sampler3D voxelRadianceMips;

layout (binding = 0) uniform atomic_uint fragListSize;
layout (binding = 0, offset = 4) uniform atomic_uint indirectFragCount;
layout (binding = 0, offset = 16) uniform atomic_uint overflowSize1;
layout (binding = 0, offset = 20) uniform atomic_uint indirectOverflowCount1;
layout (binding = 0, offset = 32) uniform atomic_uint overflowSize2;
layout (binding = 0, offset = 36) uniform atomic_uint indirectOverflowCount2;
layout (binding = 0, offset = 48) uniform atomic_uint overflowSize3;
layout (binding = 0, offset = 52) uniform atomic_uint indirectOverflowCount3;

layout (binding = 0, r32ui) uniform uimage3D voxelCounters;
layout (binding = 1, rgb10_a2ui) writeonly uniform uimage2D fragmentList;
layout (binding = 2, rgba16f) writeonly uniform image3D voxelGrid;
layout (binding = 3, rgba16f) writeonly uniform image2D voxelOverflow1;
layout (binding = 4, rgba16f) writeonly uniform image2D voxelOverflow2;
layout (binding = 5, rgba16f) writeonly uniform image2D voxelOverflow3;

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inTexCoord;
layout (location = 2) in flat int inDirection;

uniform int lightCount = 0;

layout(binding = 0, std140) uniform GLLightData {
	Light lights[MAX_LIGHTS];
};

layout(binding = 1, std140) uniform GLVoxelInfo {
	VoxelInfo voxelInfo;
};

##import lib/mirror_shadow_common

uniform float lightAttenuation = 0.5;

in vec4 gl_FragCoord;

##import lib/shading
##import voxel_trace_shared

// Data format: [radiance.r], [radiance.g], [radiance.b, count] (16 bit per color + 8 bit overflow, 8 bits count)

void main()
{
	vec4 baseColor = texture(baseColorTex, inTexCoord);
	if (baseColor.a < 0.5) discard;

	float metalness = texture(metallicTex, inTexCoord).r;

	vec3 position = vec3(gl_FragCoord.xy / VOXEL_SUPER_SAMPLE_SCALE, gl_FragCoord.z * VOXEL_GRID_SIZE);
	position = AxisSwapReverse[abs(inDirection)-1] * (position - VOXEL_GRID_SIZE / 2);
	vec3 worldPosition = position * voxelInfo.size + voxelInfo.center;
	position += VOXEL_GRID_SIZE / 2;

	vec3 pixelLuminance = DirectShading(worldPosition, baseColor.rgb, inNormal, inNormal);
	// TODO(xthexder): Fix IIR pass, buffers are cleared at this point
	// if (lightAttenuation > 0) {
	// 	vec3 directDiffuseColor = baseColor.rgb - baseColor.rgb * metalness;
	// 	vec3 indirectDiffuse = HemisphereIndirectDiffuse(worldPosition, inNormal, vec2(0));
	// 	pixelLuminance += indirectDiffuse * directDiffuseColor * lightAttenuation * smoothstep(0.0, 0.1, length(indirectDiffuse));
	// }

	uint count = imageAtomicAdd(voxelCounters, ivec3(position), 1);
	if (count == 0) {
		uint index = atomicCounterIncrement(fragListSize);
		if (index % 256 == 0) atomicCounterIncrement(indirectFragCount);
		imageStore(fragmentList, ivec2(index & MaxFragListMask[0], index >> FragListWidthBits[0]), uvec4(position, 1));
		imageStore(voxelGrid, ivec3(position), vec4(pixelLuminance, 1.0));
	} else if (count == 1) {
		uint index = atomicCounterIncrement(overflowSize1) * 2;
		if (index % 512 == 0) atomicCounterIncrement(indirectOverflowCount1);
		imageStore(voxelOverflow1, ivec2(index & MaxFragListMask[0], index >> FragListWidthBits[0]), vec4(position, 1.0));
		imageStore(voxelOverflow1, ivec2((index & MaxFragListMask[0]) + 1, index >> FragListWidthBits[0]), vec4(pixelLuminance, 1.0));
	} else if (count == 2) {
		uint index = atomicCounterIncrement(overflowSize2) * 2;
		if (index % 512 == 0) atomicCounterIncrement(indirectOverflowCount2);
		imageStore(voxelOverflow2, ivec2(index & MaxFragListMask[1], index >> FragListWidthBits[1]), vec4(position, 1.0));
		imageStore(voxelOverflow2, ivec2((index & MaxFragListMask[1]) + 1, index >> FragListWidthBits[1]), vec4(pixelLuminance, 1.0));
	} else {
		uint index = atomicCounterIncrement(overflowSize3) * 2;
		if (index % 512 == 0) atomicCounterIncrement(indirectOverflowCount3);
		imageStore(voxelOverflow3, ivec2(index & MaxFragListMask[2], index >> FragListWidthBits[2]), vec4(position, 1.0));
		imageStore(voxelOverflow3, ivec2((index & MaxFragListMask[2]) + 1, index >> FragListWidthBits[2]), vec4(pixelLuminance, 1.0));
	}
}
