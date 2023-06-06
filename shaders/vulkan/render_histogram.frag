/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#version 450

#extension GL_OVR_multiview2 : enable
layout(num_views = 2) in;

#include "../lib/histogram_common.glsl"
#include "../lib/util.glsl"

layout(binding = 0) uniform sampler2DArray luminanceTex;
layout(binding = 1, r32ui) uniform uimage2D histogram;

INCLUDE_LAYOUT(binding = 2)
#include "lib/exposure_state.glsl"

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outFragColor;

void main() {
    ivec2 size = textureSize(luminanceTex, 0).xy;
    vec3 c = texture(luminanceTex, vec3(inTexCoord, gl_ViewID_OVR)).rgb;

    float binFloat = inTexCoord.x * HistogramBins;
    int bin = int(floor(binFloat));
    uint count = imageLoad(histogram, ivec2(bin, 0)).r;
    float height = count * (histDownsample * histDownsample / histSampleScale) / float(size.x * size.y);

    float binLuminance = exposure * luminanceFromRatio(float(bin) / HistogramBins);
    float y = 1 - inTexCoord.y;
    vec2 edge = step(vec2(y), vec2(height, height + 0.005));
    vec3 bar = (edge[1] - edge[0]) * vec3(0, 0.5, 0);

    outFragColor.rgb = edge[0] * binLuminance + bar + (1 - edge[1]) * c;
}
