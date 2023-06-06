/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#define SMAA_GLSL_4 1
#define SMAA_PRESET_HIGH 1

#include "../lib/types_common.glsl"

layout(binding = 10)
#include "../vulkan/lib/view_states_uniform.glsl"

#define SMAA_RT_METRICS vec4(views[gl_ViewID_OVR].invExtents, views[gl_ViewID_OVR].extents)

#include "smaa.glsl"
