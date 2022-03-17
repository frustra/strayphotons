#version 460
#extension GL_EXT_shader_16bit_storage : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_OVR_multiview2 : enable
layout(num_views = 3) in;

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

#include "lib/voxel_data_buffers.glsl"

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

    // Write the front voxel
    uint count = imageAtomicAdd(fillCounters, ivec3(inVoxelPos), 1);
    if (count == 0) {
        uint index = atomicAdd(fragmentCount, 1);
        if (index % MipmapWorkGroupSize == 0) atomicAdd(fragmentsCmd.x, 1);
        fragmentList[index].position = u16vec3(uvec3(inVoxelPos));
        fragmentList[index].radiance = f16vec3(pixelRadiance);
        imageStore(radianceOut, ivec3(inVoxelPos), vec4(pixelRadiance, 1.0));
    } else if (count == 1) {
        uint index = atomicAdd(overflowCount[0], 1);
        if (index % MipmapWorkGroupSize == 0) atomicAdd(overflowCmd[0].x, 1);
        overflowList0[index].position = u16vec3(uvec3(inVoxelPos));
        overflowList0[index].radiance = f16vec3(pixelRadiance);
    } else if (count == 2) {
        uint index = atomicAdd(overflowCount[1], 1);
        if (index % MipmapWorkGroupSize == 0) atomicAdd(overflowCmd[1].x, 1);
        overflowList1[index].position = u16vec3(uvec3(inVoxelPos));
        overflowList1[index].radiance = f16vec3(pixelRadiance);
    } else {
        uint index = atomicAdd(overflowCount[2], 1);
        if (index % MipmapWorkGroupSize == 0) atomicAdd(overflowCmd[2].x, 1);
        overflowList2[index].position = u16vec3(uvec3(inVoxelPos));
        overflowList2[index].radiance = f16vec3(pixelRadiance);
    }
}
