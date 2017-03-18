#version 430

#ifdef PCF_ENABLED
#define USE_PCF
#endif

#define INCLUDE_MIRRORS
#define LIGHTING_GEL

##import lib/types_common

layout (binding = 0) uniform sampler2D gBuffer0;
layout (binding = 1) uniform sampler2D gBuffer1;
layout (binding = 2) uniform sampler2D gBuffer2;
layout (binding = 3) uniform sampler2D shadowMap;
layout (binding = 4) uniform sampler2DArray mirrorShadowMap;
layout (binding = 5) uniform sampler3D voxelRadiance;
layout (binding = 6) uniform sampler2D indirectDiffuseSampler;
layout (binding = 7) uniform usampler2D mirrorIndexStencil;
layout (binding = 8) uniform sampler2D lightingGel;
layout (binding = 9) uniform sampler2D aoBuffer;

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
uniform bool ssaoEnabled;

void getDepthNormal(out float depth, out vec3 normal, vec2 texCoord)
{
	normal = DecodeNormal(texture(gBuffer1, texCoord).xy);
	depth = length(texture(gBuffer2, texCoord));
}

bool detectEdge(vec3 centerNormal, float centerDepth, vec2 tcRadius)
{
	float depthU, depthR, depthD, depthL;
	vec3 normalU, normalR, normalD, normalL;

	getDepthNormal(depthU, normalU, inTexCoord + tcRadius * vec2(0, 1));
	getDepthNormal(depthR, normalR, inTexCoord + tcRadius * vec2(1, 0));
	getDepthNormal(depthD, normalD, inTexCoord + tcRadius * vec2(0, -1));
	getDepthNormal(depthL, normalL, inTexCoord + tcRadius * vec2(-1, 0));

	vec4 ntest = centerNormal * mat4x3(normalU, normalD, normalL, normalR);
	vec4 depthRatio = abs((vec4(depthU, depthR, depthD, depthL) / centerDepth) - 1.0);
	vec2 depthDiff = vec2(depthD + depthU - 2 * centerDepth, depthL + depthR - 2 * centerDepth);

	return any(bvec3(
		any(lessThan(ntest, vec4(0.8))),
		any(greaterThan(depthRatio, vec4(0.05))),
		any(greaterThan(depthDiff, vec2(0.01)))
	));
}

void main()
{
	// for (int i = 0; i < mirrorData.count[0]; i++) {
	// 	vec2 coord = inTexCoord * vec2(16.0, 9.0) - vec2(i, 0);
	// 	if (coord == clamp(coord, 0, 1)) {
	// 		outFragColor.rgb = texture(mirrorShadowMap, vec3(coord, i)).rrr;
	// 		return;
	// 	}
	// }

	vec4 gb0 = texture(gBuffer0, inTexCoord);
	vec4 gb1 = texture(gBuffer1, inTexCoord);
	vec4 gb2 = texture(gBuffer2, inTexCoord);

	vec3 baseColor = gb0.rgb;
	float roughness = gb0.a;
	vec3 viewNormal = DecodeNormal(gb1.rg);
	vec3 flatViewNormal = DecodeNormal(gb1.ba);
	float metalness = gb2.a;

	if (length(viewNormal) < 0.9) {
		outFragColor.rgb = vec3(0);
		return;
	}

	// Determine coordinates of fragment.
	vec3 fragPosition = ScreenPosToViewPos(inTexCoord, 0, invProjMat);
	vec3 viewPosition = gb2.rgb;
	vec3 worldPosition = (invViewMat * vec4(viewPosition, 1.0)).xyz;
	vec3 worldFragPosition = (invViewMat * vec4(fragPosition, 1.0)).xyz;

	vec3 worldNormal = mat3(invViewMat) * viewNormal;
	vec3 flatWorldNormal = mat3(invViewMat) * flatViewNormal;

	// Transform fragment position into mirror space if drawn through a mirror.
	uint tuple = texture(mirrorIndexStencil, inTexCoord).r;
	if (tuple > 0) {
		tuple -= 1; // Value is offset by 1 in texture.
		int mirrorId = UnpackMirrorDest(tuple);
		if (MirrorSourceIsMirror(tuple)) {
			int sourceIndex = UnpackMirrorSource(tuple);
			worldFragPosition = vec3(mirrorSData.invReflectMat[sourceIndex] * vec4(worldFragPosition, 1.0));
		}
		// Single reflection matrix is involutory so reflectMat can be used without inversion
		worldFragPosition = vec3(mirrors[mirrorId].reflectMat * vec4(worldFragPosition, 1.0));
	}

	// Trace.
	vec3 rayDir = normalize(worldPosition - worldFragPosition);
	vec3 rayReflectDir = reflect(rayDir, worldNormal);

	vec3 indirectSpecular = vec3(0);
	{
		// specular
		vec3 directSpecularColor = mix(vec3(0.04), baseColor, metalness);
		if (any(greaterThan(directSpecularColor, vec3(0.0))) && roughness < 1.0) {
			float specularConeRatio = roughness * 0.8;
			vec4 sampleColor = ConeTraceGrid(specularConeRatio, worldPosition, rayReflectDir, flatWorldNormal, gl_FragCoord.xy);

			vec3 brdf = EvaluateBRDFSpecularImportanceSampledGGX(directSpecularColor, roughness, rayReflectDir, -rayDir, worldNormal);
			indirectSpecular = sampleColor.rgb * brdf * M_PI;
		}
	}

	vec3 indirectDiffuse = vec3(0);

	if (diffuseDownsample > 1) {
		if (detectEdge(viewNormal, length(viewPosition), diffuseDownsample * 0.65 / textureSize(gBuffer0, 0))) {
			indirectDiffuse = HemisphereIndirectDiffuse(worldPosition, worldNormal, flatWorldNormal, vec2(0));
		} else {
			indirectDiffuse = texture(indirectDiffuseSampler, inTexCoord).rgb / exposure;
		}
		// Add noise back in at full resolution to remove banding
		indirectDiffuse *= 1.0 + InterleavedGradientNoise(gl_FragCoord.xy) * 0.1;
	} else {
		indirectDiffuse = texture(indirectDiffuseSampler, inTexCoord).rgb / exposure;
	}
	if (ssaoEnabled) indirectDiffuse *= vec3(texture(aoBuffer, inTexCoord).r);

	vec3 directDiffuseColor = baseColor - baseColor * metalness;
	vec3 directLight = DirectShading(worldPosition, -rayDir, baseColor, worldNormal, flatWorldNormal, roughness, metalness);

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
