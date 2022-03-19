#version 460
#extension GL_EXT_nonuniform_qualifier : enable

layout(constant_id = 0) const int FRAGMENT_LIST_COUNT = 1;

#define DIFFUSE_ONLY_SHADING
#define SHADOWS_ENABLED
#define LIGHTING_GELS

#include "../lib/types_common.glsl"
#include "../lib/util.glsl"
#include "../lib/voxel_shared.glsl"

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inVoxelPos;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) flat in int baseColorTexID;
layout(location = 5) flat in int metallicRoughnessTexID;

layout(std430, set = 3, binding = 0) buffer VoxelFragmentList {
    uint count;
    uint capacity;
    VkDispatchIndirectCommand cmd;
    VoxelFragment list[];
}
fragmentLists[MAX_VOXEL_FRAGMENT_LISTS];

INCLUDE_LAYOUT(binding = 2)
#include "lib/light_data_uniform.glsl"

layout(binding = 3) uniform sampler2D shadowMap;
layout(set = 2, binding = 0) uniform sampler2D textures[];

#include "../lib/shading.glsl"

layout(binding = 4, r32ui) uniform uimage3D fillCounters;
layout(binding = 5, rgba16f) writeonly uniform image3D radianceOut;

void main() {
    vec4 baseColor = texture(textures[baseColorTexID], inTexCoord);
    if (baseColor.a < 0.5) discard;

    vec4 metallicRoughnessSample = texture(textures[metallicRoughnessTexID], inTexCoord);
    float roughness = metallicRoughnessSample.g;
    float metalness = metallicRoughnessSample.b;

    vec3 pixelRadiance = DirectShading(inWorldPos, baseColor.rgb, inNormal, inNormal, roughness, metalness);

    uint bucket = min(FRAGMENT_LIST_COUNT, imageAtomicAdd(fillCounters, ivec3(inVoxelPos), 1));
    uint index = atomicAdd(fragmentLists[bucket].count, 1);
    if (index >= fragmentLists[bucket].capacity) return;
    if (index % MipmapWorkGroupSize == 0) atomicAdd(fragmentLists[bucket].cmd.x, 1);
    fragmentLists[bucket].list[index].position = u16vec3(uvec3(inVoxelPos));
    fragmentLists[bucket].list[index].radiance = f16vec3(pixelRadiance);

    if (bucket == 0) {
        // First fragment is written to the voxel grid directly
        imageStore(radianceOut, ivec3(inVoxelPos), vec4(pixelRadiance, 1.0));
    }
}
