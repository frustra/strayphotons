#ifndef HISTOGRAM_COMMON_INCLUDED
#define HISTOGRAM_COMMON_INCLUDED

#include "lighting_util.glsl"

#define HistogramBins 64
const float lumMin = -8, lumMax = 4;
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
