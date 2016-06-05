#version 430

##import lib/util

layout (binding = 0) uniform sampler2D gBuffer0;
layout (binding = 1) uniform sampler2D gBuffer1;
layout (binding = 2) uniform sampler2D depthStencil;
layout (binding = 3) uniform sampler2D noiseTexture;

layout (location = 0) in vec2 inTexCoord;
layout (location = 1) noperspective in vec3 inViewRay;
layout (location = 0) out vec4 outFragColor;

const int kernelSize = 24;
const float radius = 4.0;
const float power = 2.6;

uniform vec3 kernel[kernelSize];
uniform mat4 projMatrix;

void main()
{
	vec3 normalSample = texture(gBuffer1, inTexCoord).rgb;
	if (length(normalSample) < 0.5) {
		// Normal not defined.
		outFragColor = vec4(1);
		return;
	}

	// Determine view space coordinates of fragment.
	float depth = texture(depthStencil, inTexCoord).r;
	depth = LinearDepth(depth, ClippingPlane) * ClippingPlane.y;
	vec3 position = inViewRay * depth;

	// Load screen space normal in range [-1, 1].
	vec3 normal = mat3(projMatrix) * normalSample;

	// Calculate kernel rotation.
	vec2 noiseTextureScale = vec2(textureSize(depthStencil, 0)) / vec2(textureSize(noiseTexture, 0));
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
		vec4 sampleScreenSpacePos = projMatrix * vec4(sampleViewSpacePos, 1);
		vec2 sampleTexCoord = (sampleScreenSpacePos.xy / sampleScreenSpacePos.w) * 0.5 + 0.5;

		// Read screen space depth of sample.
		float sampleScreenSpaceZ = texture(depthStencil, 1-sampleTexCoord).r;
		sampleScreenSpaceZ = LinearDepth(sampleScreenSpaceZ, ClippingPlane) * ClippingPlane.y;

		// Attenuate occlusion as the sample depth diverges from the current fragment's depth.
		float distanceFalloff = smoothstep(0.0, 1.0, radius / abs(sampleScreenSpaceZ - position.z));

		// Compare with theoretical depth of sample, if the screen space depth
		// is closer then there must be an occluder.
		float occluded = step(sampleScreenSpaceZ, sampleViewSpacePos.z);
		occlusion += distanceFalloff * occluded;
	}

	occlusion = 1 - (occlusion / float(kernelSize));
	occlusion = pow(occlusion, power);

	outFragColor = vec4(vec3(occlusion), 1.0) * texture(gBuffer0, inTexCoord);
}