/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#version 450

layout(location = 0) out vec2 outTexCoord;

out gl_PerVertex {
    vec4 gl_Position;
};

vec2 positions[3] = vec2[](vec2(-2, -1), vec2(2, -1), vec2(0, 3));

vec2 uvs[3] = vec2[](vec2(-0.5, 1), vec2(1.5, 1), vec2(0.5, -1));

vec2 uvsFlipped[3] = vec2[](vec2(-0.5, 0), vec2(1.5, 0), vec2(0.5, 2));

const bool flipped = false;

void main() {
    outTexCoord = flipped ? uvsFlipped[gl_VertexIndex] : uvs[gl_VertexIndex];
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
}
