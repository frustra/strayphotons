#version 450
layout(early_fragment_tests) in;

#extension GL_OVR_multiview2 : enable
layout(num_views = 2) in;

#include "../lib/util.glsl"
#include "../lib/types_common.glsl"

layout(binding = 0) uniform sampler2D baseColorTex;
layout(binding = 1) uniform sampler2D metallicRoughnessTex;

layout(location = 0) in vec3 inViewPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec4 gBuffer0; // rgba8
layout(location = 1) out vec4 gBuffer1; // rgba16f
layout(location = 2) out vec4 gBuffer2; // rgba16f

void main() {
	vec4 baseColor = texture(baseColorTex, inTexCoord);
	if (baseColor.a < 0.5) discard;

	vec4 metallicRoughnessSample = texture(metallicRoughnessTex, inTexCoord);
	float roughness = metallicRoughnessSample.g;
	float metallic = metallicRoughnessSample.b;

	gBuffer0.rgb = baseColor.rgb;
	gBuffer0.a = roughness;
	gBuffer1.rg = EncodeNormal(inNormal);
	gBuffer1.ba = vec2(0);
	gBuffer2.rgb = inViewPos;
	gBuffer2.a = metallic;
}
