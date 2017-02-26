#version 430

// 0-3 are model textures
layout (binding = 4) uniform sampler2D shadowMap;
layout (binding = 5) uniform sampler2DArray mirrorShadowMap;

layout (location = 0) in vec3 inViewPos;

##import lib/util
##import lib/types_common
##import lib/mirror_shadow_common

#define MIRROR_SAMPLE
##import lib/shadow_sample
#undef MIRROR_SAMPLE
##import lib/shadow_sample

uniform int drawMirrorId;

uniform int lightCount = 0;

layout(binding = 0, std140) uniform LightData {
	Light lights[MAX_LIGHTS];
};

layout (location = 0) out vec4 gBuffer0;

void main()
{
	uint tuple = mirrorData.list[gl_Layer];
	int sourceId = UnpackMirrorSource(tuple);
	int mirrorId = UnpackMirrorDest(tuple);

	if (drawMirrorId >= 0) {
		uint mask = 1 << uint(drawMirrorId);
		uint prevValue = atomicOr(mirrorData.mask[gl_Layer], mask);
		if ((prevValue & mask) == 0) {
			uint index = atomicAdd(mirrorData.count[0], 1);
			mirrorData.list[index] = PackMirrorAndMirror(gl_Layer, drawMirrorId);
			mirrorData.sourceLight[index] = mirrorData.sourceLight[gl_Layer];
		}
	}

	vec4 ndcPosOnMirror = mirrorData.projMat[gl_Layer] * vec4(inViewPos, 1.0);
	ndcPosOnMirror /= ndcPosOnMirror.w;
	ndcPosOnMirror.z = -1;
	vec4 viewPosOnMirror = mirrorData.invProjMat[gl_Layer] * ndcPosOnMirror;
	viewPosOnMirror /= viewPosOnMirror.w;
	vec4 worldPosOnMirror = mirrorData.invViewMat[gl_Layer] * viewPosOnMirror;

	float occlusion;
	ShadowInfo info;

	if (MirrorSourceIsMirror(tuple)) {
		vec4 mirrorViewPosOnMirror = mirrorData.viewMat[sourceId] * worldPosOnMirror;

		info.index = sourceId;
		info.shadowMapPos = mirrorViewPosOnMirror.xyz;
		info.projMat = mirrorData.projMat[sourceId];
		info.invProjMat = mirrorData.invProjMat[sourceId];
		info.mapOffset = vec4(0, 0, 1, 1);
		info.clip = mirrorData.clip[sourceId];
		//info.nearInfo = mirrorData.nearInfo[sourceId]
		occlusion = SimpleOcclusionMirror(info);
	} else {
		Light light = lights[sourceId];
		vec4 lightViewPosOnMirror = light.view * worldPosOnMirror;

		info.shadowMapPos = lightViewPosOnMirror.xyz;
		info.projMat = light.proj;
		info.invProjMat = light.invProj;
		info.mapOffset = light.mapOffset;
		info.clip = light.clip;
		occlusion = SimpleOcclusion(info);
	}

	gBuffer0.r = (occlusion * 0.5 + 0.5) * LinearDepth(inViewPos, mirrorData.clip[gl_Layer]);
}
