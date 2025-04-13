/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

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
layout(location = 5) flat in float emissiveScale;
layout(location = 6) flat in uvec4 decalIDs;

layout(location = 0) out vec4 gBaseColor; // rgba8srgb
layout(location = 1) out vec4 gNormalEmissive; // rgba16f
layout(location = 2) out vec2 gRoughnessMetalic; // rg8unorm
layout(location = 3) out uvec4 gDecals; // rgba8uint

void main() {
    vec4 baseColor = texture(textures[baseColorTexID], inTexCoord);

    vec4 metallicRoughnessSample = texture(textures[metallicRoughnessTexID], inTexCoord);
    float roughness = metallicRoughnessSample.g;
    float metallic = metallicRoughnessSample.b;

    gBaseColor.rgba = baseColor.rgba;
    gNormalEmissive.rg = EncodeNormal(inNormal);
    gNormalEmissive.b = emissiveScale;
    gRoughnessMetalic.r = roughness;
	gRoughnessMetalic.g = metallic;
	gDecals.rgba = decalIDs;
}
