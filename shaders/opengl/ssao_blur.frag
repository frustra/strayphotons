#version 430

##import lib/util

layout (binding = 0) uniform sampler2D occlusionTex;
layout (binding = 1) uniform sampler2D gBuffer2;
layout (location = 0) in vec2 inTexCoord;
layout (location = 0) out vec4 outFragColor;

uniform vec2 samplePattern;
const int radius = 10;
const float sharpness = 10.0;

// Bottom/left corner of sample area.
const float offset = float(-radius) * 0.5 + 0.5;

uniform mat4 invProjMat;

void main() {
	vec3 fragPos = texture(gBuffer2, inTexCoord).rgb; // view space

	// Sum of samples.
	float accum = 0.0;
	float totalWeight = 1e-6;

	for (int index = 0; index < radius; index++) {
		vec2 relativeCoord = vec2(offset * 0.5 + float(index));

		vec2 sampleCoord = relativeCoord * samplePattern + inTexCoord;
		vec3 samplePos = texture(gBuffer2, sampleCoord).rgb; // view space

		float ddepth = (fragPos.z - samplePos.z) * sharpness;
		float weight = exp2(- ddepth * ddepth);

		float occlusion = texture(occlusionTex, sampleCoord).r;
		accum += occlusion * weight;
		totalWeight += weight;
	}

	outFragColor.r = accum / totalWeight;
}