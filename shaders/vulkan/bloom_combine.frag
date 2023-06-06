/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#version 450

#extension GL_OVR_multiview2 : enable
layout(num_views = 2) in;

layout(binding = 0) uniform sampler2DArray luminanceTex;
layout(binding = 1) uniform sampler2DArray blurTex1;
layout(binding = 2) uniform sampler2DArray blurTex2;

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outFragColor;

const float weight1 = 0.4;
const float weight2 = 0.5;

void main() {
    vec3 lum = texture(luminanceTex, vec3(inTexCoord, gl_ViewID_OVR)).rgb;
    if (any(isnan(lum))) lum = vec3(0.0);

    vec3 bloom1 = texture(blurTex1, vec3(inTexCoord, gl_ViewID_OVR)).rgb;
    vec3 bloom2 = texture(blurTex2, vec3(inTexCoord, gl_ViewID_OVR)).rgb;

    outFragColor = vec4(lum + bloom1 * weight1 + bloom2 * weight2, 1.0);
}
