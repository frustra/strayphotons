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

layout (location = 0) in vec2 inTexCoord;
layout (location = 0) out vec4 outFragColor;

##import lib/light_inputs

#define INCLUDE_VOXEL_TRACE

uniform float voxelSize = 0.1;
uniform vec3 voxelGridCenter = vec3(0);

uniform float exposure = 0.1;
uniform vec2 targetSize;

##import lib/util
##import voxel_shared
##import lib/shading

const int maxReflections = 2;

uniform mat4 invViewMat;
uniform mat4 invProjMat;

uniform int mode = 1;

const int diffuseAngles = 6;

vec3 orientByNormal(float phi, float tht, vec3 normal)
{
	float sintht = sin(tht);
	float xs = sintht * cos(phi);
	float ys = cos(tht);
	float zs = sintht * sin(phi);

	vec3 up = abs(normal.y) < 0.999 ? vec3(0, 1, 0) : vec3(1, 0, 0);
	vec3 tangent1 = normalize(up - normal * dot(up, normal));
	vec3 tangent2 = normalize(cross(tangent1, normal));
	return normalize(xs * tangent1 + ys * normal + zs * tangent2);
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
	vec3 directionFromView = normalize(viewPosition);
	vec3 worldPosition = (invViewMat * vec4(viewPosition, 1.0)).xyz;
	vec3 worldFragPosition = (invViewMat * vec4(fragPosition, 1.0)).xyz;

	vec3 worldNormal = mat3(invViewMat) * viewNormal;

	// Trace.
	vec3 rayDir = normalize(worldPosition - worldFragPosition);
	vec3 rayReflectDir = reflect(rayDir, worldNormal);

	vec3 indirectSpecular = vec3(0);
	vec4 indirectDiffuse = vec4(0);

	vec3 directLight = DirectShading(worldPosition, -rayDir, baseColor, worldNormal, roughness, metalness);

	vec3 directDiffuseColor = baseColor - baseColor * metalness;
	vec3 directSpecularColor = mix(vec3(0.04), baseColor, metalness);

	for (int i = 0; i < maxReflections; i++) {
		// specular
		vec3 sampleDir = rayReflectDir;
		vec4 sampleColor = ConeTraceGrid(roughness, worldPosition, sampleDir, worldNormal);

		if (roughness == 0 && sampleColor.a >= 0) {
			worldPosition += sampleDir * (sampleColor.a - 0.5 * voxelSize);
			vec3 voxelPos = (worldPosition - voxelGridCenter) / voxelSize + VoxelGridSize * 0.5;
			GetVoxel(voxelPos, 0, baseColor, worldNormal, directLight, roughness);
			rayDir = sampleDir;
			rayReflectDir = reflect(sampleDir, worldNormal);
		} else {
			vec3 brdf = EvaluateBRDF(vec3(0), directSpecularColor, roughness, sampleDir, -rayDir, worldNormal);
			indirectSpecular = sampleColor.rgb * brdf;
			break;
		}
	}

	for (float r = 0; r < diffuseAngles; r++) {
		for (float a = 0.3; a <= 0.9; a += 0.3) {
			// diffuse
			vec3 sampleDir = orientByNormal(r / diffuseAngles * 6.28, a, worldNormal);
			vec4 sampleColor = ConeTraceGridDiffuse(worldPosition, sampleDir, worldNormal);

			vec3 brdf = BRDF_Diffuse_Lambert(directDiffuseColor);
			indirectDiffuse += sampleColor * dot(sampleDir, worldNormal) * vec4(brdf, 1.0);
		}
	}

	indirectDiffuse /= diffuseAngles;

	vec3 totalLight = directLight + indirectDiffuse.rgb + indirectSpecular;

	if (mode == 0) { // Direct only
		outFragColor = vec4(directLight * exposure, 1.0);
	} else if (mode == 2) { // Indirect lighting
		totalLight = roughness * indirectDiffuse.rgb * baseColor + (1.0 - roughness) * indirectSpecular;
		outFragColor = vec4(totalLight * exposure, 1.0);
	} else if (mode == 3) { // diffuse
		outFragColor = vec4(indirectDiffuse.rgb, 1.0);
	} else if (mode == 4) { // specular
		outFragColor = vec4(indirectSpecular, 1.0);
	} else { // Full lighting
		outFragColor = vec4(totalLight * exposure, 1.0);
	}
}
