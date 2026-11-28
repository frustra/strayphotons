/*
 * Stray Photons - Copyright (C) 2026 Jacob Wirth & Justine Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#version 430

// #define DEBUG_OVEREXPOSED
#define ENABLE_DITHER

#extension GL_OVR_multiview2 : enable
layout(num_views = 2) in;

#include "../lib/lighting_util.glsl"
#include "../lib/util.glsl"

layout(binding = 0) uniform sampler2DArray luminanceTex;

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outFragColor;

layout(constant_id = 0) const float WHITE_POINT_R = 1.0;
layout(constant_id = 1) const float WHITE_POINT_G = 1.0;
layout(constant_id = 2) const float WHITE_POINT_B = 1.0;
layout(constant_id = 4) const int HDR_MODE = 0;
layout(constant_id = 5) const float DARK_SATURATION = 0.75;
layout(constant_id = 6) const float BRIGHT_SATURATION = 1.1;
const float ditherAmount = 1.0 / 255.0;

void main() {
    vec4 luminosity = texture(luminanceTex, vec3(inTexCoord, gl_ViewID_OVR)); // pre-exposed

    vec3 linearToneMapped = HableTonemap(max(vec3(0), luminosity.rgb)) /
                            HableTonemap(vec3(WHITE_POINT_R, WHITE_POINT_G, WHITE_POINT_B));

    vec3 hsv = RGBtoHSV(linearToneMapped);
    float brightness = clamp(hsv.z, 0, 1);
    float x = (brightness - 1);
    float darkMix = 1 - x * x * x * x;
    hsv.y = clamp(hsv.y, 0, 1) * clamp(mix(DARK_SATURATION, BRIGHT_SATURATION, darkMix), 0, 1);
    linearToneMapped = HSVtoRGB(hsv);

    vec3 toneMapped;
    if (HDR_MODE == 7 /* HLG */) {
        const float a = 0.17883277;
        const float b = 1.0 - 4.0 * a;
        const float c = 0.5 - a * log(4.0 * a);

        vec3 toneMappedGamma = 0.5 * sqrt(linearToneMapped);
        vec3 toneMappedLog = a * log(linearToneMapped - b) + c;
        toneMapped = mix(toneMappedGamma, toneMappedLog, greaterThan(linearToneMapped, vec3(1)));
    } else {
        // Gamma correction is automatic for sRGB textures
        toneMapped = linearToneMapped;
    }
    outFragColor = vec4(toneMapped, luminosity.a);

#ifdef ENABLE_DITHER
    outFragColor += (vec4(InterleavedGradientNoise(gl_FragCoord.xy),
                         InterleavedGradientNoise(gl_FragCoord.xy * 2),
                         InterleavedGradientNoise(gl_FragCoord.xy * 3),
                         InterleavedGradientNoise(gl_FragCoord.xy * 5)) -
                        0.5) *
                    ditherAmount;
#endif

#ifdef DEBUG_OVEREXPOSED
    if (toneMapped.r > 1 || toneMapped.g > 1 || toneMapped.b > 1) {
        // Highlight overexposed/blown out areas.
        outFragColor = vec4(1, 0, 0, 1);
    }
#endif
}
