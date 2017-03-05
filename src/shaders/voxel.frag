#version 430

#define DIFFUSE_ONLY_SHADING
#define INCLUDE_MIRRORS
#define LIGHTING_GEL

##import lib/util
##import voxel_shared

##import lib/types_common

layout (binding = 0) uniform sampler2D baseColorTex;
layout (binding = 1) uniform sampler2D roughnessTex;
// binding 2 = metallicTex
// binding 3 = heightTex
layout (binding = 4) uniform sampler2D shadowMap;
layout (binding = 5) uniform sampler2DArray mirrorShadowMap;
layout (binding = 6) uniform sampler2D lightingGel;

layout (binding = 0) uniform atomic_uint fragListSize;
layout (binding = 0, offset = 4) uniform atomic_uint nextComputeSize;

layout (binding = 0, r32ui) writeonly uniform uimage2D voxelFragList;
layout (binding = 1, r32ui) uniform uimage3D voxelData;

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inTexCoord;
layout (location = 2) in flat int inDirection;

uniform int lightCount = 0;

layout(binding = 0, std140) uniform GLLightData {
	Light lights[MAX_LIGHTS];
};

##import lib/mirror_shadow_common

uniform float voxelSize = 0.1;
uniform vec3 voxelGridCenter = vec3(0);

in vec4 gl_FragCoord;

##import lib/shading

// Data format: [radiance.r], [radiance.g], [radiance.b, count] (24 bit per color, 8 bits count)

void main()
{
	vec4 baseColor = texture(baseColorTex, inTexCoord);
	if (baseColor.a < 0.5) discard;

	float roughness = texture(roughnessTex, inTexCoord).r;

	vec3 position = vec3(gl_FragCoord.xy / VoxelSuperSampleScale, gl_FragCoord.z * VoxelGridSize);
	position = AxisSwapReverse[abs(inDirection)-1] * (position - VoxelGridSize / 2);
	vec3 worldPosition = position * voxelSize + voxelGridCenter;
	position += VoxelGridSize / 2;

	vec3 pixelLuminance = DirectShading(worldPosition, baseColor.rgb, inNormal, inNormal, roughness);

	// Clip so we don't overflow, scale to 16 bits
	uvec3 radiance = uvec3(min(vec3(1.0), pixelLuminance) * 0xFFFF);

	ivec3 dataOffset = ivec3(floor(position.x) * 3, position.yz);
	imageAtomicAdd(voxelData, dataOffset + ivec3(0, 0, 0), radiance.r);
	imageAtomicAdd(voxelData, dataOffset + ivec3(1, 0, 0), radiance.g);
	uint prevData = imageAtomicAdd(voxelData, dataOffset + ivec3(2, 0, 0), (radiance.b << 8) + 1);

	if ((prevData & 0xFF) == 0) {
		uint index = atomicCounterIncrement(fragListSize);
		if (index % MipmapWorkGroupSize == 0) atomicCounterIncrement(nextComputeSize);

		uint packedData = (uint(position.x) & 0x3FF) << 20;
		packedData += (uint(position.y) & 0x3FF) << 10;
		packedData += uint(position.z) & 0x3FF;
		imageStore(voxelFragList, ivec2(index & MaxFragListMask[0], index >> FragListWidthBits[0]), uvec4(packedData));
	}
}
