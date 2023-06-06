/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

// Separable Gaussian filter, template fragment shader
layout(binding = 0) uniform SAMPLER_TYPE sourceTex;

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outFragColor;

#ifdef SIZE_LARGE
const int size = 15;
// clang-format off
float kernel[size] = float[](
    //0.009033, 0.018476, 0.033851, 0.055555, 0.08167, 0.107545, 0.126854, 0.134032, 0.126854, 0.107545, 0.08167, 0.055555, 0.033851, 0.018476, 0.009033
    //0.000489, 0.002403, 0.009246, 0.02784, 0.065602, 0.120999, 0.174697, 0.197448, 0.174697, 0.120999, 0.065602, 0.02784, 0.009246, 0.002403, 0.000489
    0.000137, 0.000971, 0.005087, 0.019712, 0.056514, 0.119899, 0.188269, 0.218824, 0.188269, 0.119899, 0.056514, 0.019712, 0.005087, 0.000971, 0.000137
);
// clang-format on
#endif

#ifdef SIZE_SMALL
const int size = 3;
// clang-format off
float kernel[size] = float[](
	//0.153388, 0.221461, 0.250301, 0.221461, 0.153388
	0.319466, 0.361069, 0.319466
);
// clang-format on
#endif

const int left = (size - 1) / 2;

layout(push_constant) uniform PushConstants {
    vec2 direction;
    float threshold;
    float scale;
};

void main() {
    vec4 sum = vec4(0.0);
    vec2 texsize = textureSize(sourceTex, 0).xy;

    SAMPLE_COORD_TYPE originCoord = SAMPLE_COORD(inTexCoord);

    for (int i = 0; i < size; i++) {
        vec2 offset = (float(i - left) * direction * 2.0) / texsize;
        SAMPLE_COORD_TYPE coord = originCoord;
        coord.xy += offset;
        vec4 px = min(texture(sourceTex, coord), vec4(threshold));
        sum += px * kernel[i];
    }

    outFragColor = vec4(sum.rgb * scale, 1);
}
