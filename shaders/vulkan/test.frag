#version 450

#extension GL_OVR_multiview2 : enable
layout(num_views = 2) in;

#include "../lib/lighting_util.glsl"
#include "../lib/types_common.glsl"

layout(binding = 0) uniform sampler2D baseColorTex;
layout(binding = 1) uniform sampler2D metallicRoughnessTex;
layout(binding = 2) uniform sampler2D shadowMap;

layout(location = 0) in vec3 viewPosition;
layout(location = 1) in vec3 viewNormal;
layout(location = 2) in vec3 color;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) out vec4 outColor;

layout(binding = 11, std140) uniform LightData {
	Light lights[MAX_LIGHTS];
	int lightCount;
};

layout(binding = 10) uniform ViewStates {
	ViewState views[2];
};

#define SHADOWS_ENABLED 1
#define USE_PCF 1
#include "../lib/shading.glsl"

void main() {
	ViewState view = views[gl_ViewID_OVR];

	vec3 fragPosition = ScreenPosToViewPos(gl_FragCoord.xy / view.extents, 0, view.invProjMat);
	vec3 worldPosition = (view.invViewMat * vec4(viewPosition, 1.0)).xyz;
	vec3 worldFragPosition = (view.invViewMat * vec4(fragPosition, 1.0)).xyz;
	vec3 rayDir = normalize(worldPosition - worldFragPosition);

	vec3 worldNormal = mat3(view.invViewMat) * normalize(viewNormal);
	//vec3 flatWorldNormal = mat3(view.invViewMat) * flatViewNormal;

	vec4 gb0 = texture(baseColorTex, inTexCoord);
	vec3 baseColor = gb0.rgb;
	if (gb0.a < 0.5) discard;
	if (dot(-viewPosition, viewNormal) <= 0) discard;

	vec4 metallicRoughnessSample = texture(metallicRoughnessTex, inTexCoord);
	float roughness = metallicRoughnessSample.g;
	float metalness = metallicRoughnessSample.b;

	vec3 directLight = DirectShading(worldPosition, -rayDir, baseColor, worldNormal, worldNormal, roughness, metalness);
	outColor = vec4(directLight, 1.0);
}
