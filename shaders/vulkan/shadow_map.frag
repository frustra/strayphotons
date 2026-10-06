/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justine Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#version 430

layout(early_fragment_tests) in; // Force depth/stencil testing before shader invocation.

#include "../lib/util.glsl"

layout(location = 0) in vec3 inViewPos;
layout(location = 1) flat in uint inRenderableIndex;
layout(location = 0) out float outLinearDepth;

#include "../lib/types_common.glsl"

INCLUDE_LAYOUT(binding = 0)
#include "lib/view_states_uniform.glsl"

layout(constant_id = 0) const uint TRACKED_LIGHT_INDEX = ~0;

layout(binding = 1) buffer RenderableVisibility {
    uint renderableCount;
    uint renderableBuckets[];
};

void main() {
    outLinearDepth = LinearDepth(inViewPos, views[0].clip);
    if (TRACKED_LIGHT_INDEX < ~0) {
        atomicAdd(renderableBuckets[inRenderableIndex + TRACKED_LIGHT_INDEX * renderableCount], 1);
    }
}
