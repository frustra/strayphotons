#version 430

#define USE_PCF
#define INCLUDE_MIRRORS

##import lib/types_common

layout (binding = 0) uniform sampler2D gBuffer0;
layout (binding = 1) uniform sampler2D gBuffer1;
layout (binding = 2) uniform sampler2D gBuffer2;
layout (binding = 3) uniform sampler2D gBuffer3;
layout (binding = 4) uniform sampler2D shadowMap;
layout (binding = 5) uniform sampler2DArray mirrorShadowMap;

layout (binding = 6) uniform sampler3D voxelColor;
layout (binding = 7) uniform sampler3D voxelNormal;
layout (binding = 8) uniform sampler3D voxelRadiance;

layout (binding = 9) uniform sampler2D indirectDiffuseSampler;

layout (binding = 10) uniform usampler2D mirrorIndexStencil;

layout (location = 0) in vec2 inTexCoord;
layout (location = 0) out vec4 outFragColor;

uniform int lightCount = 0;
uniform int mirrorCount = 0;

layout(binding = 0, std140) uniform GLLightData {
	Light lights[MAX_LIGHTS];
};

layout(binding = 1, std140) uniform GLMirrorData {
	Mirror mirrors[MAX_MIRRORS];
};

##import lib/mirror_shadow_common
##import lib/mirror_scene_common

uniform float voxelSize = 0.1;
uniform vec3 voxelGridCenter = vec3(0);

uniform float exposure = 1.0;
uniform float diffuseDownsample = 1;

##import lib/util
##import voxel_shared
##import voxel_trace_shared
##import lib/shading

uniform mat4 invViewMat;
uniform mat4 invProjMat;

uniform int mode = 1;

void getDepthNormal(out float depth, out vec3 normal, vec2 texCoord)
{
	normal = texture(gBuffer1, texCoord).xyz;
	depth = texture(gBuffer3, texCoord).z;
}

bool detectEdge(vec3 centerNormal, float centerDepth, vec2 tcRadius)
{
	return false; // TODO(jli): fix
	float depthU, depthR, depthD, depthL;
	vec3 normalU, normalR, normalD, normalL;

	getDepthNormal(depthU, normalU, inTexCoord + tcRadius * vec2(0, 1));
	getDepthNormal(depthR, normalR, inTexCoord + tcRadius * vec2(1, 0));
	getDepthNormal(depthD, normalD, inTexCoord + tcRadius * vec2(0, -1));
	getDepthNormal(depthL, normalL, inTexCoord + tcRadius * vec2(-1, 0));

	vec4 ntest = centerNormal * mat4x3(normalU, normalD, normalL, normalR);
	vec4 depthRatio = vec4(depthU, depthR, depthD, depthL) / centerDepth;
	vec2 depthDiff = vec2(depthD + depthU - 2 * centerDepth, depthL + depthR - 2 * centerDepth);

	return any(bvec4(
		any(lessThan(ntest, vec4(0.8))),
		any(greaterThan(depthRatio, vec4(1.001))),
		any(lessThan(depthRatio, vec4(0.999))),
		any(greaterThan(depthDiff, vec2(1e-4)))
	));
}

void main()
{
	//for (int i = 0; i < mirrorData.count[0]; i++) {
	//	vec2 coord = inTexCoord * vec2(6.0, 4.0) - vec2(i, 0);
	//	if (coord == clamp(coord, 0, 1)) {
	//		outFragColor.rgb = texture(mirrorShadowMap, vec3(coord, i)).rrr;
	//		return;
	//	}
	//}

	vec4 gb0 = texture(gBuffer0, inTexCoord);
	vec4 gb1 = texture(gBuffer1, inTexCoord);
	vec4 gb2 = texture(gBuffer2, inTexCoord);
	vec4 gb3 = texture(gBuffer3, inTexCoord);

	vec3 baseColor = gb0.rgb;
	float roughness = gb0.a;
	vec3 viewNormal = gb1.rgb;
	vec3 flatViewNormal = gb2.rgb;
	float metalness = gb2.a;

	// Determine coordinates of fragment.
	vec3 fragPosition = ScreenPosToViewPos(inTexCoord, 0, invProjMat);
	vec3 viewPosition = gb3.rgb;
	vec3 worldPosition = (invViewMat * vec4(viewPosition, 1.0)).xyz;
	vec3 worldFragPosition = (invViewMat * vec4(fragPosition, 1.0)).xyz;

	vec3 worldNormal = mat3(invViewMat) * viewNormal;
	vec3 flatWorldNormal = mat3(invViewMat) * flatViewNormal;

	// Trace.
	uint tuple = texture(mirrorIndexStencil, inTexCoord).r;
	int mirrorId = UnpackMirrorDest(tuple);
	if (mirrorId < mirrorCount)
	{
		worldFragPosition = vec3(mirrors[mirrorId].reflectMat * vec4(worldFragPosition, 1.0));
		if (MirrorSourceIsMirror(tuple)) {
			int sourceIndex = UnpackMirrorSource(tuple);
			worldFragPosition = vec3(mirrorSData.reflectMat[sourceIndex] * vec4(worldFragPosition, 1.0));
		}
	}
	vec3 rayDir = normalize(worldPosition - worldFragPosition);
	vec3 rayReflectDir = reflect(rayDir, worldNormal);
	float reflected = 0.0;

	vec3 indirectSpecular;
	{
		// specular
		float specularConeRatio = roughness * 0.8;
		vec4 sampleColor = ConeTraceGrid(specularConeRatio, worldPosition, rayReflectDir, flatWorldNormal, gl_FragCoord.xy);

		vec3 directSpecularColor = mix(vec3(0.04), baseColor, metalness);
		vec3 brdf = EvaluateBRDFSpecularImportanceSampledGGX(directSpecularColor, roughness, rayReflectDir, -rayDir, worldNormal);
		indirectSpecular = sampleColor.rgb * brdf;
	}

	vec3 indirectDiffuse;

	if (diffuseDownsample > 1 && detectEdge(viewNormal, viewPosition.z, diffuseDownsample * 0.65 / textureSize(gBuffer0, 0)) || reflected == 1.0 || mode == 5) {
		indirectDiffuse = HemisphereIndirectDiffuse(worldPosition, worldNormal, gl_FragCoord.xy);
	} else {
		indirectDiffuse = texture(indirectDiffuseSampler, inTexCoord).rgb / exposure;
	}

	vec3 directDiffuseColor = baseColor - baseColor * metalness;
	vec3 directLight = DirectShading(worldPosition - reflected*rayDir*voxelSize*0.75, -rayDir, baseColor, worldNormal, flatWorldNormal, roughness, metalness);

	vec3 indirectLight = indirectDiffuse * directDiffuseColor + indirectSpecular;
	vec3 totalLight = directLight + indirectLight;

	if (mode == 0) { // Direct only
		outFragColor = vec4(directLight, 1.0);
	} else if (mode == 2) { // Indirect lighting
		outFragColor = vec4(indirectLight, 1.0);
	} else if (mode == 3) { // diffuse
		outFragColor = vec4(indirectDiffuse, 1.0);
	} else if (mode == 4) { // specular
		outFragColor = vec4(indirectSpecular, 1.0);
	} else { // Full lighting
		outFragColor = vec4(totalLight, 1.0);
	}

	outFragColor.rgb *= exposure;
}
