#version 430

// 0-3 are model textures
layout (binding = 4) uniform sampler2D shadowMap;

layout (location = 0) in vec3 inViewPos;

##import lib/util
##import lib/types_common
##import lib/mirror_common
##import lib/shadow_sample

uniform int mirrorId;

uniform int lightCount = 0;

layout(binding = 0, std140) uniform LightData {
	Light lights[MAX_LIGHTS];
};

layout (location = 0) out vec4 gBuffer0;

void main()
{
	// if (mirrorId >= 0) {
	// 	uint lightId = (mirrorData.list[gl_Layer] >> 16) & 0xFFFF;
	// 	uint mask = 1 << mirrorId;
	// 	uint prevValue = atomicOr(mirrorData.mask[lightId], mask);
	// 	if ((prevValue & mask) == 0) {
	// 		uint index = atomicAdd(mirrorData.count, 1);
	// 		mirrorData.list[index] = lightId << 16 + mirrorId;
	// 	}
	// }

	uint lightId = (mirrorData.list[gl_Layer] >> 16) & 0xFFFF;
	Light light = lights[lightId];

	vec4 ndcPosOnMirror = mirrorData.projMat[gl_Layer] * vec4(inViewPos, 1.0);
	ndcPosOnMirror /= ndcPosOnMirror.w;
	ndcPosOnMirror.z = -1;
	vec4 viewPosOnMirror = mirrorData.invProjMat[gl_Layer] * ndcPosOnMirror;
	viewPosOnMirror /= viewPosOnMirror.w;
	vec4 worldPosOnMirror = mirrorData.invViewMat[gl_Layer] * viewPosOnMirror;
	vec4 lightViewPosOnMirror = light.view * worldPosOnMirror;

	ShadowInfo info = ShadowInfo(
		int(lightId),
		lightViewPosOnMirror.xyz,
		light.proj,
		light.invProj,
		light.mapOffset,
		light.clip,
		vec4(0)
	);

	float occlusion = SimpleOcclusion(info);
	gBuffer0.r = (step(0.8, occlusion) * 0.1 + 0.9) * LinearDepth(inViewPos, mirrorData.clip[gl_Layer]);
}
