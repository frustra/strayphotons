/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#version 430

layout(early_fragment_tests) in; // Force depth/stencil testing before shader invocation.

#include "../lib/util.glsl"

layout(location = 0) in flat uint inOpticID;

#include "../lib/types_common.glsl"

layout(binding = 1) writeonly buffer OpticVisibility {
    uint visibility[MAX_LIGHTS][MAX_OPTICS];
};

layout(push_constant) uniform PushConstants {
    uint lightIndex;
};

void main() {
    atomicOr(visibility[lightIndex][inOpticID - 1], 1);
}
