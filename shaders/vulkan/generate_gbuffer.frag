#version 460
#extension GL_EXT_shader_16bit_storage : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_OVR_multiview2 : enable
layout(num_views = 2) in;
layout(early_fragment_tests) in;

#include "../lib/types_common.glsl"
#include "../lib/util.glsl"

layout(set = 2, binding = 0) uniform sampler2D textures[];

layout(location = 0) in vec3 inViewPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) flat in int baseColorTexID;
layout(location = 4) flat in int metallicRoughnessTexID;

layout(location = 0) out vec4 gBuffer0; // rgba8
layout(location = 1) out vec4 gBuffer1; // rgba16f
layout(location = 2) out vec4 gBuffer2; // rgba16f

void main() {
    vec4 baseColor = texture(textures[baseColorTexID], inTexCoord);
    if (baseColor.a < 0.5) discard;

    vec4 metallicRoughnessSample = texture(textures[metallicRoughnessTexID], inTexCoord);
    float roughness = metallicRoughnessSample.g;
    float metallic = metallicRoughnessSample.b;

    gBuffer0.rgb = baseColor.rgb;
    gBuffer0.a = roughness;
    gBuffer1.rg = EncodeNormal(inNormal);
    gBuffer1.ba = vec2(0);
    gBuffer2.rgb = inViewPos;
    gBuffer2.a = metallic;
}
