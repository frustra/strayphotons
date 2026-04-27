/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
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

layout(constant_id = 0) const float WHITE_POINT_R = 8.0;
layout(constant_id = 1) const float WHITE_POINT_G = 8.0;
layout(constant_id = 2) const float WHITE_POINT_B = 8.0;
layout(constant_id = 3) const float CURVE_SCALE = 2.9;
layout(constant_id = 4) const float DARK_SATURATION = 0.7;
layout(constant_id = 5) const float BRIGHT_SATURATION = 1.2;
const float ditherAmount = 1.0 / 255.0;

void main() {
    vec4 luminosity = texture(luminanceTex, vec3(inTexCoord, gl_ViewID_OVR)); // pre-exposed

    vec3 toneMapped = HableSDRTonemap(max(vec3(0), luminosity.rgb) * CURVE_SCALE) /
                      HableSDRTonemap(vec3(WHITE_POINT_R, WHITE_POINT_G, WHITE_POINT_B));

#ifdef ENABLE_DITHER
    toneMapped += (InterleavedGradientNoise(gl_FragCoord.xy) - 0.5) * ditherAmount;
#endif

    vec3 hsv = RGBtoHSV(toneMapped);
    float brightness = clamp(hsv.z, 0, 1);
    float darkMix = (2 - brightness) * brightness;
    hsv.y = clamp(hsv.y, 0, 1) * clamp(mix(DARK_SATURATION, BRIGHT_SATURATION, darkMix), 0, 1);
    outFragColor = vec4(HSVtoRGB(hsv), luminosity.a);

#ifdef DEBUG_OVEREXPOSED
    if (toneMapped.r > 1 || toneMapped.g > 1 || toneMapped.b > 1) {
        // Highlight overexposed/blown out areas.
        outFragColor = vec4(1, 0, 0, 1);
    }
#endif
}
