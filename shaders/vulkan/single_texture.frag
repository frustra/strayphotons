/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justine Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#version 450

layout(early_fragment_tests) in;

#include "../lib/lighting_util.glsl"

layout(binding = 0) uniform sampler2D tex;

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in vec4 inColor;

layout(location = 0) out vec4 outFragColor;

layout(constant_id = 0) const int HDR_MODE = 0;
layout(constant_id = 1) const float WHITE_POINT = 0.5;

void main() {
    vec4 linearColor = inColor * texture(tex, inTexCoord);
    if (HDR_MODE == 7 /* HLG */) {
        const float a = 0.17883277;
        const float b = 1.0 - 4.0 * a;
        const float c = 0.5 - a * log(4.0 * a);

        vec3 toneMappedGamma = WHITE_POINT * sqrt(linearColor.rgb);
        vec3 toneMappedLog = a * log(linearColor.rgb - b) + c;
        outFragColor = vec4(mix(toneMappedGamma, toneMappedLog, greaterThan(linearColor.rgb, vec3(1))), linearColor.a);
    } else if (HDR_MODE == -7 /* Inverse HLG */) {
        const float a = 0.17883277;
        const float b = 1.0 - 4.0 * a;
        const float c = 0.5 - a * log(4.0 * a);

        vec3 toneMappedGamma = linearColor.rgb * linearColor.rgb / (WHITE_POINT * WHITE_POINT);
        vec3 toneMappedLog = b + exp((linearColor.rgb - c) / a);
        vec3 toneMapped = mix(toneMappedGamma, toneMappedLog, greaterThan(linearColor.rgb, vec3(WHITE_POINT)));
        outFragColor = vec4(toneMapped * 0.25, linearColor.a);
    } else {
        // Gamma correction is automatic for sRGB textures
        outFragColor = linearColor;
    }
}
