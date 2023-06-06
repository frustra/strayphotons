/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#version 450

#extension GL_OVR_multiview2 : enable
layout(num_views = 2) in;

layout(binding = 0) uniform sampler2DArray tex;
layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outFragColor;

void main() {
    outFragColor = texture(tex, vec3(inTexCoord, gl_ViewID_OVR));
}
