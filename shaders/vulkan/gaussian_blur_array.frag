/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#version 450

#extension GL_OVR_multiview2 : enable
layout(num_views = 2) in;

#define SAMPLER_TYPE sampler2DArray
#define SAMPLE_COORD_TYPE vec3
#define SAMPLE_COORD(coord) vec3(coord, gl_ViewID_OVR)
#define SIZE_LARGE

#include "gaussian_blur.frag.glsl"
