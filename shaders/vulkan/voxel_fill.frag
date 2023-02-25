#version 460
#extension GL_EXT_nonuniform_qualifier : enable

layout(constant_id = 0) const int FRAGMENT_LIST_COUNT = 1;
layout(constant_id = 1) const float LIGHT_ATTENUATION = 0.5;

#define DIFFUSE_ONLY_SHADING
#define SHADOWS_ENABLED
#define LIGHTING_GELS
#define USE_PCF

#include "../lib/types_common.glsl"
#include "../lib/util.glsl"
#include "../lib/voxel_shared.glsl"

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inVoxelPos;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) flat in int baseColorTexID;
layout(location = 5) flat in int metallicRoughnessTexID;
layout(location = 6) flat in float emissiveScale;

layout(binding = 1) uniform VoxelStateUniform {
    VoxelState voxelInfo;
};

INCLUDE_LAYOUT(binding = 2)
#include "lib/light_data_uniform.glsl"

layout(binding = 3) uniform sampler2D shadowMap;
layout(set = 2, binding = 0) uniform sampler2D textures[];

#include "../lib/shading.glsl"

layout(binding = 4, r32ui) uniform uimage3D fillCounters;
layout(binding = 5, rgba16f) writeonly uniform image3D radianceOut;
layout(binding = 6, rgba16f) writeonly uniform image3D normalsOut;

struct FragmentListMetadata {
    uint count;
    uint capacity;
    uint offset;
    VkDispatchIndirectCommand cmd;
};

layout(std430, binding = 7) buffer VoxelFragmentListMetadata {
    FragmentListMetadata fragmentListMetadata[MAX_VOXEL_FRAGMENT_LISTS];
};

layout(std430, binding = 8) buffer VoxelFragmentList {
    VoxelFragment fragmentLists[];
};

layout(binding = 9) uniform PreviousVoxelStateUniform {
    VoxelState previousVoxelInfo;
};

layout(binding = 10) uniform sampler3D voxelLayersIn[6];

void main() {
    vec4 baseColor = texture(textures[baseColorTexID], inTexCoord);
    if (baseColor.a < 0.5) discard;

    vec4 metallicRoughnessSample = texture(textures[metallicRoughnessTexID], inTexCoord);
    float roughness = metallicRoughnessSample.g;
    float metalness = metallicRoughnessSample.b;

    vec3 pixelRadiance = DirectShading(inWorldPos, baseColor.rgb, inNormal, inNormal, roughness, metalness);
    pixelRadiance += emissiveScale * baseColor.rgb;

    if (LIGHT_ATTENUATION > 0) {
        vec3 voxelPos = (previousVoxelInfo.worldToVoxel * vec4(inWorldPos, 1.0)).xyz;
        vec3 voxelNormal = normalize(mat3(previousVoxelInfo.worldToVoxel) * inNormal);

        vec3 indirectDiffuse = vec3(0);
        for (int i = 0; i < 6; i++) {
            vec4 sampleValue = texelFetch(voxelLayersIn[i], ivec3(voxelPos + voxelNormal), 0);
            indirectDiffuse += sampleValue.rgb * step(0, dot(AxisDirections[i], voxelNormal));
        }
        vec3 directDiffuseColor = baseColor.rgb * (1 - metalness);
        pixelRadiance += indirectDiffuse * directDiffuseColor * LIGHT_ATTENUATION *
                         smoothstep(0.0, 0.1, length(indirectDiffuse));
    }

    uint bucket = min(FRAGMENT_LIST_COUNT, imageAtomicAdd(fillCounters, ivec3(inVoxelPos), 1));
    uint index = atomicAdd(fragmentListMetadata[bucket].count, 1);
    if (index >= fragmentListMetadata[bucket].capacity) return;
    if (index % MipmapWorkGroupSize == 0) atomicAdd(fragmentListMetadata[bucket].cmd.x, 1);

    uint listOffset = fragmentListMetadata[bucket].offset + index;
    fragmentLists[listOffset].position = u16vec3(uvec3(inVoxelPos));
    fragmentLists[listOffset].radiance = f16vec3(pixelRadiance);
    fragmentLists[listOffset].normal = f16vec3(inNormal);

    if (bucket == 0) {
        // First fragment is written to the voxel grid directly
        imageStore(radianceOut, ivec3(inVoxelPos), vec4(pixelRadiance, 1.0));
        imageStore(normalsOut, ivec3(inVoxelPos), vec4(inNormal, 1.0));
    }
}
