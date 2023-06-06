/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#version 450

#define SAMPLER_TYPE sampler2D
#define SAMPLE_COORD_TYPE vec2
#define SAMPLE_COORD(coord) coord
#define SIZE_SMALL

#include "gaussian_blur.frag.glsl"
