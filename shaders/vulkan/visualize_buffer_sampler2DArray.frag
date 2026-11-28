/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justine Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#version 450

layout(binding = 0) uniform sampler2DArray tex;
layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform PushConstants {
    float layer;
};

// 2 bits per channel swizzle
// 0bAABBGGRR
// RR=0, GG=1, BB=2, AA=3 => rgba = rgba
// 1 bit per channel mask
// 0b0000ABGR
layout(constant_id = 0) const uint SWIZZLE_MASK = 0x0FE4;

void main() {
    const vec4 clearColor = vec4(0, 0, 0, 1);
    vec4 value = texture(tex, vec3(inTexCoord, layer));
    outFragColor = vec4(mix(clearColor[SWIZZLE_MASK & 0x3], value[SWIZZLE_MASK & 0x3], bool(SWIZZLE_MASK & 0x0100)),
        mix(clearColor[(SWIZZLE_MASK & 0xC) >> 2], value[(SWIZZLE_MASK & 0xC) >> 2], bool(SWIZZLE_MASK & 0x0200)),
        mix(clearColor[(SWIZZLE_MASK & 0x30) >> 4], value[(SWIZZLE_MASK & 0x30) >> 4], bool(SWIZZLE_MASK & 0x0400)),
        mix(clearColor[(SWIZZLE_MASK & 0xC0) >> 6], value[(SWIZZLE_MASK & 0xC0) >> 6], bool(SWIZZLE_MASK & 0x0800)));
}
