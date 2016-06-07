#version 430

##import lib/util

layout (binding = 0) uniform sampler2D occlusionTex;
layout (binding = 1) uniform sampler2D gBuffer1;
layout (binding = 2) uniform sampler2D depthStencil;
layout (binding = 3) uniform sampler2D gBuffer0;
layout (location = 0) in vec2 inTexCoord;
layout (location = 0) out vec4 outFragColor;

uniform bool combineOutput;
uniform vec2 samplePattern;
const int radius = 4;

void main() {
	float fragDepth = texture(depthStencil, inTexCoord).r;
	fragDepth = LinearDepth(fragDepth, ClippingPlane) * ClippingPlane.y;
	vec3 fragNormal = texture(gBuffer1, inTexCoord).rgb;

	if (length(fragNormal) < 0.5) {
		// Normal not defined.
		outFragColor = vec4(1);
		return;
	}

	// Sum of samples.
	float accum = 0.0;
	float samples = 0.0;

	// Bottom/left corner of sample area.
	float offset = float(-radius) * 0.5 + 0.5;

	for (int index = 0; index < radius; index++) {
		vec2 relativeCoord = vec2(offset + float(index));

		vec2 sampleCoord = relativeCoord * samplePattern + inTexCoord;
		vec3 sampleNormal = texture(gBuffer1, sampleCoord).rgb;

		float sampleDepth = texture(depthStencil, sampleCoord).r;
		sampleDepth = LinearDepth(sampleDepth, ClippingPlane) * ClippingPlane.y;

		float depthRatio = fragDepth/sampleDepth;

		if (depthRatio < 2 && depthRatio > 0.5 && dot(fragNormal, sampleNormal) > 0.5) {
			float occlusion = texture(occlusionTex, sampleCoord).r;
			accum += occlusion;
			samples += 1;
		}
	}

	accum /= samples;

	if (combineOutput) {
		vec4 baseColor = texture(gBuffer0, inTexCoord);
		outFragColor = vec4(baseColor.rgb * accum, baseColor.a);
	} else {
		outFragColor.rgb = vec3(accum);
	}
}