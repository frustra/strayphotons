#version 430

#define USE_PCF

layout (binding = 0) uniform sampler2D gBuffer0;
layout (binding = 1) uniform sampler2D gBuffer1;
layout (binding = 2) uniform sampler2D depthStencil;
layout (binding = 3) uniform sampler2D shadowMap;

layout (binding = 4) uniform sampler3D voxelColor;
layout (binding = 5) uniform sampler3D voxelNormal;
layout (binding = 6) uniform sampler3D voxelAlpha;
layout (binding = 7) uniform sampler3D voxelRadiance;

layout (binding = 8) uniform sampler2D indirectDiffuseSampler;

layout (location = 0) in vec2 inTexCoord;
layout (location = 0) out vec4 outFragColor;

##import lib/light_inputs

uniform float voxelSize = 0.1;
uniform vec3 voxelGridCenter = vec3(0);

uniform float exposure = 1.0;
uniform vec2 targetSize;
uniform float diffuseDownsample = 1;

##import lib/util
##import voxel_shared
##import voxel_trace_shared
##import lib/shading

const int maxReflections = 2;

uniform mat4 invViewMat;
uniform mat4 invProjMat;

uniform int mode = 1;

bool compareEdge(vec3 centerNormal, float centerDepth, vec2 texCoord)
{
	vec3 normal = texture(gBuffer1, texCoord).xyz;
	float depth = texture(depthStencil, texCoord).x;

	if (dot(normal, centerNormal) < 0.8)
		return true;

	float depthRatio = depth / centerDepth;
	return depthRatio < 0.999 || depthRatio > 1.001;
}

bool detectEdge(vec3 centerNormal, float centerDepth, vec2 tcRadius)
{
	if (compareEdge(centerNormal, centerDepth, inTexCoord + tcRadius * vec2(1, 0)))
		return true;
	if (compareEdge(centerNormal, centerDepth, inTexCoord + tcRadius * vec2(1, 1)))
		return true;
	if (compareEdge(centerNormal, centerDepth, inTexCoord + tcRadius * vec2(0, 1)))
		return true;
	if (compareEdge(centerNormal, centerDepth, inTexCoord + tcRadius * vec2(-1, 1)))
		return true;
	if (compareEdge(centerNormal, centerDepth, inTexCoord + tcRadius * vec2(-1, 0)))
		return true;
	if (compareEdge(centerNormal, centerDepth, inTexCoord + tcRadius * vec2(-1, -1)))
		return true;
	if (compareEdge(centerNormal, centerDepth, inTexCoord + tcRadius * vec2(0, -1)))
		return true;
	if (compareEdge(centerNormal, centerDepth, inTexCoord + tcRadius * vec2(1, -1)))
		return true;
	return false;
}

void main()
{
	// Determine normal of surface at this fragment.
	vec4 gb1 = texture(gBuffer1, inTexCoord);
	vec3 viewNormal = gb1.rgb;
	if (length(viewNormal) < 0.5) {
		// Normal not defined.
		outFragColor = vec4(0);
		return;
	}

	vec4 gb0 = texture(gBuffer0, inTexCoord);
	vec3 baseColor = gb0.rgb;
	float roughness = gb0.a;
	float metalness = gb1.a;

	// Determine coordinates of fragment.
	float depth = texture(depthStencil, inTexCoord).r;
	vec3 fragPosition = ScreenPosToViewPos(inTexCoord, 0, invProjMat);
	vec3 viewPosition = ScreenPosToViewPos(inTexCoord, depth, invProjMat);
	vec3 worldPosition = (invViewMat * vec4(viewPosition, 1.0)).xyz;
	vec3 worldFragPosition = (invViewMat * vec4(fragPosition, 1.0)).xyz;

	vec3 worldNormal = mat3(invViewMat) * viewNormal;

	// Trace.
	vec3 rayDir = normalize(worldPosition - worldFragPosition);
	vec3 rayReflectDir = reflect(rayDir, worldNormal);

	if (mode == 5) { // Voxel direct lighting
		vec4 sampleColor = ConeTraceGrid(0, worldFragPosition, rayDir, rayDir);
		worldPosition = worldFragPosition + rayDir * sampleColor.a;
		vec3 voxelPos = (worldPosition - voxelGridCenter) / voxelSize + VoxelGridSize * 0.5;
		GetVoxel(voxelPos, 0, baseColor, worldNormal, sampleColor.rgb, roughness);

		rayReflectDir = reflect(rayDir, worldNormal);
	}

	vec3 indirectDiffuse;

	if (diffuseDownsample > 1 && detectEdge(viewNormal, depth, diffuseDownsample * 0.65 / textureSize(gBuffer0, 0))) {
		vec3 directDiffuseColor = baseColor - baseColor * metalness;
		indirectDiffuse = HemisphereIndirectDiffuse(directDiffuseColor, worldPosition, worldNormal);
	} else {
		indirectDiffuse = texture(indirectDiffuseSampler, inTexCoord).rgb / exposure;
	}

	vec3 indirectSpecular = vec3(0);

	for (int i = 0; i < maxReflections; i++) {
		// specular
		vec3 sampleDir = rayReflectDir;
		vec4 sampleColor = ConeTraceGrid(roughness, worldPosition, sampleDir, worldNormal);

		if (roughness == 0 && metalness == 1 && sampleColor.a >= 0) {
			worldPosition += sampleDir * sampleColor.a;
			vec3 voxelPos = (worldPosition - voxelGridCenter) / voxelSize + VoxelGridSize * 0.5;
			vec3 radiance;
			GetVoxel(voxelPos, 0, baseColor, worldNormal, radiance, roughness);
			rayDir = sampleDir;
			rayReflectDir = reflect(sampleDir, worldNormal);
			if (roughness != 0) metalness = 0;
		} else {
			vec3 directSpecularColor = mix(vec3(0.04), baseColor, metalness);
			vec3 brdf = EvaluateBRDFSpecularImportanceSampledGGX(directSpecularColor, roughness, sampleDir, -rayDir, worldNormal);
			indirectSpecular = sampleColor.rgb * brdf;
			break;
		}
	}

	vec3 directLight = DirectShading(worldPosition, -rayDir, baseColor, worldNormal, roughness, metalness);

	vec3 indirectLight = indirectDiffuse + indirectSpecular;
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
