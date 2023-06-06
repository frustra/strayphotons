/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef HISTOGRAM_COMMON_INCLUDED
#define HISTOGRAM_COMMON_INCLUDED

#include "lighting_util.glsl"

#define HistogramBins 128
const float lumMin = -15, lumMax = 6;
const float powLumMin = pow(2, lumMin);
const float histSampleScale = 10.0;
const float histDownsample = 2.0;

float luminanceFromRatio(float ratio) {
    return exp2(ratio * (lumMax - lumMin) + lumMin) - powLumMin;
}

float ratioFromPixel(vec3 c) {
    float lum = DigitalLuminance(c);
    return saturate((log2(lum) - lumMin) / (lumMax - lumMin));
}

float histogramBinFromPixel(vec3 c) {
    return float(HistogramBins - 1) * ratioFromPixel(c);
}

#endif
