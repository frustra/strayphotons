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

layout(binding = 4, rgba16f) writeonly uniform image3D radianceOut;

void main() {
    vec4 baseColor = texture(textures[baseColorTexID], inTexCoord);
    if (baseColor.a < 0.5) discard;

    vec4 metallicRoughnessSample = texture(textures[metallicRoughnessTexID], inTexCoord);
    float roughness = metallicRoughnessSample.g;
    float metalness = metallicRoughnessSample.b;

    vec3 pixelLuminance = DirectShading(inWorldPos, baseColor.rgb, inNormal, inNormal, roughness, metalness);
    imageStore(radianceOut, ivec3(inVoxelPos), vec4(pixelLuminance, 1.0));
}
