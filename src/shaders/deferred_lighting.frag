#version 430

#define USE_PCF

##import lib/util
##import lib/lighting_util

layout (binding = 0) uniform sampler2D gBuffer0;
layout (binding = 1) uniform sampler2D gBuffer1;
layout (binding = 2) uniform sampler2D depthStencil;
layout (binding = 3) uniform sampler2D shadowMap;

layout (location = 0) in vec2 inTexCoord;
layout (location = 0) out vec4 outFragColor;

##import lib/light_inputs

uniform float exposure = 1.0;
uniform vec2 targetSize;

uniform mat4 viewMat;
uniform mat4 invViewMat;
uniform mat4 invProjMat;

##import lib/shadow_sample

void main() {
	float depth = texture(depthStencil, inTexCoord).r;

	vec4 gb0 = texture(gBuffer0, inTexCoord);
	vec3 baseColor = gb0.rgb;
	float roughness = gb0.a;

	vec4 gb1 = texture(gBuffer1, inTexCoord);
	vec3 normal = mat3(invViewMat) * gb1.rgb;
	float metalness = gb1.a;

	// Unproject depth to reconstruct viewPosition in view-space.
	vec3 viewPosition = ScreenPosToViewPos(inTexCoord, depth, invProjMat);
	vec3 worldPosition = (invViewMat * vec4(viewPosition, 1.0)).xyz;

	vec3 directionToView = normalize((invViewMat * vec4(vec3(0), 1)).xyz - worldPosition);

	vec3 pixelLuminance = directShading(worldPosition, directionToView, baseColor, normal, roughness, metalness);

	// Pre-scale luminance by exposure to avoid exceeding MAX_FLOAT16.
	vec3 exposedLuminance = pixelLuminance * exposure;
	outFragColor = vec4(exposedLuminance, 1);
}
