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
	vec3 worldPosition = position * voxelSize + voxelGridCenter;
	position += VOXEL_GRID_SIZE / 2;

	vec3 pixelLuminance = DirectShading(worldPosition, baseColor.rgb, inNormal, inNormal);
	if (lightAttenuation > 0) {
		vec3 directDiffuseColor = baseColor.rgb - baseColor.rgb * metalness;
		vec3 indirectDiffuse = HemisphereIndirectDiffuse(worldPosition, inNormal, vec2(0));
		pixelLuminance += indirectDiffuse * directDiffuseColor * lightAttenuation * smoothstep(0.0, 0.1, length(indirectDiffuse));
	}

	// Scale to 10 bits 0-1, clamp to 16 bit for HDR
	uvec3 radiance = uvec3(clamp(pixelLuminance, 0, VoxelFixedPointExposure) * 0x3FF);

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
