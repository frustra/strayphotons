/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef COLOR_UTIL_GLSL_INCLUDED
#define COLOR_UTIL_GLSL_INCLUDED

// Maps a single channel of linear color to sRGB space.
float LinearToSRGBChannel(float linear) {
    if (linear < 0.00313067) return linear * 12.92;
    return pow(linear, (1.0 / 2.4)) * 1.055 - 0.055;
}

// Maps a linear RGB color to sRGB space.
vec3 LinearToSRGB(vec3 linear) {
    return vec3(LinearToSRGBChannel(linear.r), LinearToSRGBChannel(linear.g), LinearToSRGBChannel(linear.b));
}

// Maps a single channel of sRGB color to linear space.
float SRGBToLinearChannel(float srgb) {
    if (srgb <= 0.04045) return srgb * (1.0 / 12.92);
    return pow(srgb * (1.0 / 1.055) + 0.0521327, 2.4);
}

// Maps a linear RGB color to sRGB space.
vec3 SRGBToLinear(vec3 srgb) {
    return vec3(SRGBToLinearChannel(srgb.r), SRGBToLinearChannel(srgb.g), SRGBToLinearChannel(srgb.b));
}

// Maps a linear RGB color to sRGB space.
// http://chilliant.blogspot.ca/2012/08/srgb-approximations-for-hlsl.html
vec3 FastLinearToSRGB(vec3 linear) {
    vec3 S1 = sqrt(linear);
    vec3 S2 = sqrt(S1);
    vec3 S3 = sqrt(S2);
    return 0.662002687 * S1 + 0.684122060 * S2 - 0.323583601 * S3 - 0.0225411470 * linear;
}

// Maps an sRGB color to linear space.
// http://chilliant.blogspot.ca/2012/08/srgb-approximations-for-hlsl.html
vec3 FastSRGBToLinear(vec3 srgb) {
    return srgb * (srgb * (srgb * 0.305306011 + 0.682171111) + 0.012522878);
}

// HSV-related color space functions
// http://www.chilliant.com/rgb2hsv.html
vec3 HUEtoRGB(in float H) {
    float R = abs(H * 6 - 3) - 1;
    float G = 2 - abs(H * 6 - 2);
    float B = 2 - abs(H * 6 - 4);
    return saturate(vec3(R, G, B));
}

vec3 RGBtoHCV(vec3 RGB) {
    vec4 P = (RGB.g < RGB.b) ? vec4(RGB.bg, -1.0, 2.0 / 3.0) : vec4(RGB.gb, 0.0, -1.0 / 3.0);
    vec4 Q = (RGB.r < P.x) ? vec4(P.xyw, RGB.r) : vec4(RGB.r, P.yzx);
    float C = Q.x - min(Q.w, Q.y);
    float H = abs((Q.w - Q.y) / (6 * C + 1e-10) + Q.z);
    return vec3(H, C, Q.x);
}

vec3 HSVtoRGB(in vec3 HSV) {
    vec3 RGB = HUEtoRGB(HSV.x);
    return ((RGB - 1) * HSV.y + 1) * HSV.z;
}

vec3 RGBtoHSV(in vec3 RGB) {
    vec3 HCV = RGBtoHCV(RGB);
    float S = HCV.y / (HCV.z + 1e-10);
    return vec3(HCV.x, S, HCV.z);
}

#endif
