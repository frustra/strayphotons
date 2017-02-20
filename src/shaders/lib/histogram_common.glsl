#ifndef HISTOGRAM_COMMON_INCLUDED
#define HISTOGRAM_COMMON_INCLUDED

##import lib/lighting_util

#define HistogramBins 64
const float lumMin = -8, lumMax = 4;
const float histSampleScale = 10.0;
const float histDownsample = 2.0;

float histogramBinFromPixel(vec3 c) {
	float lum = DigitalLuminance(c);
	float ratio = saturate((log2(lum) - lumMin) / (lumMax - lumMin));
	return float(HistogramBins - 1) * ratio;
}

#endif