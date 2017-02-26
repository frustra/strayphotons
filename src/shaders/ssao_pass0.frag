#version 430

##import lib/util

layout (binding = 0) uniform sampler2D gBuffer0;
layout (binding = 1) uniform sampler2D gBuffer1;
layout (binding = 2) uniform sampler2D gBuffer3;
layout (binding = 3) uniform sampler2D noiseTexture;

layout (location = 0) in vec2 inTexCoord;
layout (location = 0) out vec4 outFragColor;

const int kernelSize = 24;
const float radius = 0.4;
const float power = 2.2;

uniform vec3 kernel[kernelSize];
uniform mat4 projMat;
uniform mat4 invProjMat;

void main()
{
	vec3 normalSample = texture(gBuffer1, inTexCoord).rgb;
	vec3 position = texture(gBuffer3, inTexCoord).rgb; // view space

	// Load screen space normal in range [-1, 1].
	vec3 normal = mat3(projMat) * normalSample;
	normal.z *= -1;

	// Calculate kernel rotation.
	vec2 noiseTextureScale = vec2(textureSize(gBuffer3, 0)) / vec2(textureSize(noiseTexture, 0));
	vec3 rotation = texture(noiseTexture, inTexCoord * noiseTextureScale).xyz * 2 - 1;

	// Compute 3 orthonomal vectors to construct a change-of-basis matrix that
	// rotates the kernel along the fragment's normal.
	vec3 tangent1 = normalize(rotation - ProjectVec3(rotation, normal)); // One iteration of Gram-Schmidt.
	vec3 tangent2 = cross(normal, tangent1);
	mat3 basis = mat3(tangent1, tangent2, normal) * radius;

	float occlusion = 0.0;

	for (int i = 0; i < kernelSize; i++) {
		vec3 sampleViewSpacePos = basis * kernel[i] + position;

		// Project sample into screen space.
		vec4 sampleScreenSpacePos = projMat * vec4(sampleViewSpacePos, 1);
		vec2 sampleTexCoord = (sampleScreenSpacePos.xy / sampleScreenSpacePos.w) * 0.5 + 0.5;
		vec3 samplePosition = texture(gBuffer3, sampleTexCoord).rgb;

		// Attenuate occlusion as the sample depth diverges from the current fragment's depth.
		float distanceFalloff = smoothstep(0.0, 1.0, radius / abs(samplePosition.z - position.z));

		// Compare with theoretical depth of sample, if the screen space depth
		// is closer then there must be an occluder.
		float occluded = step(sampleViewSpacePos.z, samplePosition.z);
		occlusion += distanceFalloff * occluded;
	}

	occlusion = 1.000001 - (occlusion / float(kernelSize));
	occlusion = pow(occlusion, power);

	outFragColor = vec4(vec3(occlusion), 1.0);
}
