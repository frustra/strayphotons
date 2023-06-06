/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#version 450

layout(location = 0) in vec2 inPos;

layout(push_constant) uniform PushConstants {
    mat4 projMat;
}
constants;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    gl_Position = constants.projMat * vec4(inPos, 0.0, 1.0);
}
