#version 460
#extension GL_EXT_shader_16bit_storage : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_OVR_multiview2 : enable
layout(num_views = 3) in;

#include "../lib/types_common.glsl"
#include "../lib/util.glsl"

layout(set = 2, binding = 0) uniform sampler2D textures[];

layout(location = 0) in vec3 inViewPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) flat in int baseColorTexID;
layout(location = 4) flat in int metallicRoughnessTexID;

layout (binding = 0, rgba16f) writeonly uniform image3D radianceOut;

void main() {
    vec4 baseColor = texture(textures[baseColorTexID], inTexCoord);
    if (baseColor.a < 0.5) discard;

    // vec4 metallicRoughnessSample = texture(textures[metallicRoughnessTexID], inTexCoord);
    // float roughness = metallicRoughnessSample.g;
    // float metallic = metallicRoughnessSample.b;

	imageStore(radianceOut, ivec3(inViewPos * 0.5 + 0.5), vec4(baseColor.rgb, 1.0));
}
