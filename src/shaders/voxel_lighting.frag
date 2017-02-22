#version 430

#define USE_PCF
#define INCLUDE_MIRRORS

##import lib/types_common

layout (binding = 0) uniform sampler2D gBuffer0;
layout (binding = 1) uniform sampler2D gBuffer1;
layout (binding = 2) uniform sampler2D gBuffer2;
layout (binding = 3) uniform sampler2D depthStencil;
layout (binding = 4) uniform sampler2D shadowMap;
layout (binding = 5) uniform sampler2DArray mirrorShadowMap;

layout (binding = 6) uniform sampler3D voxelColor;
layout (binding = 7) uniform sampler3D voxelNormal;
layout (binding = 8) uniform sampler3D voxelRadiance;

layout (binding = 9) uniform sampler2D indirectDiffuseSampler;

layout (location = 0) in vec2 inTexCoord;
layout (location = 0) out vec4 outFragColor;

uniform int lightCount = 0;

layout(binding = 0, std140) uniform LightData {
	Light lights[MAX_LIGHTS];
};

##import lib/mirror_common

uniform float voxelSize = 0.1;
uniform vec3 voxelGridCenter = vec3(0);

uniform float exposure = 1.0;
uniform float diffuseDownsample = 1;

##import lib/util
##import voxel_shared
##import voxel_trace_shared2
##import lib/shading

const int maxReflections = 2;

uniform mat4 invViewMat;
uniform mat4 invProjMat;

uniform int mode = 1;

void getDepthNormal(out float depth, out vec3 normal, vec2 texCoord)
{
	normal = texture(gBuffer1, texCoord).xyz;
	depth = texture(depthStencil, texCoord).x;
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
	vec4 gb0 = texture(gBuffer0, inTexCoord);
	vec4 gb1 = texture(gBuffer1, inTexCoord);
	vec4 gb2 = texture(gBuffer2, inTexCoord);
	vec3 baseColor = gb0.rgb;
	float roughness = gb0.a;
	vec3 viewNormal = gb1.rgb;
	vec3 flatViewNormal = gb2.rgb;
	float metalness = gb2.a;

	// Determine coordinates of fragment.
	float depth = texture(depthStencil, inTexCoord).r;
	vec3 fragPosition = ScreenPosToViewPos(inTexCoord, 0, invProjMat);
	vec3 viewPosition = ScreenPosToViewPos(inTexCoord, depth, invProjMat);
	vec3 worldPosition = (invViewMat * vec4(viewPosition, 1.0)).xyz;
	vec3 worldFragPosition = (invViewMat * vec4(fragPosition, 1.0)).xyz;

	vec3 worldNormal = mat3(invViewMat) * viewNormal;
	vec3 flatWorldNormal = mat3(invViewMat) * flatViewNormal;

	// Trace.
	vec3 rayDir = normalize(worldPosition - worldFragPosition);
	vec3 rayReflectDir = reflect(rayDir, worldNormal);
	float reflected = 0.0;

	if (mode == 5) { // Voxel direct lighting
		vec4 sampleColor = ConeTraceGrid(0, worldFragPosition, rayDir, rayDir, gl_FragCoord.xy);
		worldPosition = worldFragPosition + rayDir * sampleColor.a;
		vec3 voxelPos = (worldPosition - voxelGridCenter) / voxelSize + VoxelGridSize * 0.5;
		GetVoxel(voxelPos, 0, baseColor, worldNormal, sampleColor.rgb, roughness);
		flatWorldNormal = worldNormal;
		reflected = 1.0;

		rayReflectDir = reflect(rayDir, worldNormal);
	}

	vec3 indirectSpecular = vec3(0);

	for (int i = 0; i < maxReflections; i++) {
		// specular
		vec3 sampleDir = rayReflectDir;
		float specularConeRatio = roughness * 0.8;
		vec4 sampleColor = ConeTraceGrid(specularConeRatio, worldPosition, sampleDir, flatWorldNormal, gl_FragCoord.xy);

		if (roughness == 0 && metalness == 1 && sampleColor.a >= 0) {
			worldPosition += sampleDir * sampleColor.a;
			vec3 voxelPos = (worldPosition - voxelGridCenter) / voxelSize + VoxelGridSize * 0.5;
			vec3 radiance;
			GetVoxel(voxelPos, 0, baseColor, worldNormal, radiance, roughness);
			rayDir = sampleDir;
			rayReflectDir = reflect(sampleDir, worldNormal);
			flatWorldNormal = worldNormal;
			if (roughness != 0) metalness = 0;
			reflected = 1.0;
		} else {
			vec3 directSpecularColor = mix(vec3(0.04), baseColor, metalness);
			vec3 brdf = EvaluateBRDFSpecularImportanceSampledGGX(directSpecularColor, roughness, sampleDir, -rayDir, worldNormal);
			indirectSpecular = sampleColor.rgb * brdf;
			break;
		}
	}

	vec3 indirectDiffuse;

	if (diffuseDownsample > 1 && detectEdge(viewNormal, depth, diffuseDownsample * 0.65 / textureSize(gBuffer0, 0)) || reflected == 1.0 || mode == 5) {
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
