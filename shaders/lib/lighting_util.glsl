#ifndef LIGHTING_UTIL_GLSL_INCLUDED
#define LIGHTING_UTIL_GLSL_INCLUDED

#define LOW_DETAIL_LIGHT 0
#define HIGH_DETAIL_LIGHT 1
#define CUBE_MAP_LIGHT 2

vec3 HDRTonemap(vec3 x) {
	// Based on the Uncharted 2 implementation of a filmic tone map by Kodak [Hable 2010]

	const float A = 0.15;
	const float B = 0.50;
	const float C = 0.10;
	const float D = 0.20;
	const float E = 0.02;
	const float F = 0.30;

	return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

float DigitalLuminance(vec3 linear) {
	// ITU-R Recommendation BT. 601

	return dot(linear, vec3(0.299, 0.587, 0.114));
}

float ManualEV100(float aperture, float shutterTime, float iso) {
	// Derivation from [Lagarde/Rousiers 2014]

	// EV defined as:
	//   2^EV_s = N^2 / t and EV_s = EV_100 + log2(S/100)

	// This gives:
	//                 EV_s = log2(N^2 / t)
	// EV_100 + log2(S/100) = log2(N^2 / t)
	//               EV_100 = log2(N^2 / t) - log2(S/100)
	//               EV_100 = log2(N^2 / t * 100 / S)

	return log2((aperture * aperture / shutterTime) * (100.0 / iso));
}

float ExposureFromEV100(float ev100) {
	// Derivation from [Lagarde/Rousiers 2014]
	// Reference: http://en.wikipedia.org/wiki/Film_speed

	// maxLum = 78 / (S * q) * N^2 / t
	//        = 78 / (S * q) * 2^EV_100
	//        = 78 / (100 * 0.65) * 2^EV_100
	//        = 1.2 * 2^EV

	float maxLuminance = 1.2 * pow(2.0, ev100);
	return 1.0 / maxLuminance;
}

vec3 BRDF_Diffuse_Lambert(vec3 diffuseColor) {
	return diffuseColor * (1.0 / M_PI);
}

// Trowbridge-Reitz NDF with the GGX normalization factor
// [Blinn 1997, "Models of Light Reflection for Computer Synthesized Pictures"]
// [Walter 2007, "Microfacet Models for Refraction through Rough Surfaces"]
// [Burley (Disney) 2012, "Practical Physically Based Shading in Film and Game Production"]
float BRDF_D_GGX(float roughness, float NdotH) {
	float alpha = roughness * roughness;
	float alphaSq = alpha * alpha;
	float d = NdotH * NdotH * (alphaSq - 1) + 1;
	return saturate(alphaSq / (M_PI * d * d));
}

// [Blinn 1977, "Models of light reflection for computer synthesized pictures"]
float BRDF_D_Blinn(float roughness, float NdotH) {
	float alpha = roughness * roughness;
	float alphaSq = alpha * alpha;
	float n = 2 / alphaSq - 2;
	return (n + 2) / (2 * M_PI) * pow(NdotH, n);
}

// Tuned to match behavior of Smith visibility approximation [Karis 2013, "Real Shading in Unreal Engine 4"]
// Includes denominator of base BRDF simplified in; Vis = G / (4 * NdotL * NdotV).
// [Schlick 1994, "An Inexpensive BRDF Model for Physically-Based Rendering"]
float BRDF_V_Schlick(float roughness, float NdotV, float NdotL) {
	float r = roughness + 1;
	float k = r * r * 0.125;

	// Calculate both terms G'(l) and G'(v) at once.
	float visV = NdotV * (1 - k) + k;
	float visL = NdotL * (1 - k) + k;
	return 0.25 / (visL * visV);
}

// Schlick approximation of the Fresnel term.
// [Schlick 1994, "An Inexpensive BRDF Model for Physically-Based Rendering"]
// [Karis 2013, "Real Shading in Unreal Engine 4"] (note, not using same spherical Gaussian approximation but still relevant)
vec3 BRDF_F_Schlick(vec3 specularColor, float VdotH) {
	float Fc = pow(1 - VdotH, 5);
	return specularColor + (1 - specularColor) * Fc;
}

#endif
