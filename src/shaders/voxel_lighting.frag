#version 430

layout (binding = 0) uniform sampler2D lastOutput;
layout (binding = 1) uniform sampler2D gBuffer0;
layout (binding = 2) uniform sampler2D gBuffer1;
layout (binding = 3) uniform sampler2D depthStencil;
layout (binding = 4) uniform sampler3D voxelColor;
layout (binding = 5) uniform sampler3D voxelNormal;
layout (binding = 6) uniform sampler3D voxelAlpha;
layout (binding = 7) uniform sampler3D voxelRadiance;

layout (location = 0) in vec2 inTexCoord;
layout (location = 0) out vec4 outFragColor;

#define INCLUDE_VOXEL_TRACE

uniform float voxelSize = 0.1;
uniform vec3 voxelGridCenter = vec3(0);

##import lib/util
##import lib/lighting_util
##import voxel_shared

const int maxReflections = 2;

uniform mat4 viewMat;
uniform mat4 invViewMat;
uniform mat3 invViewRotMat;
uniform mat4 projMat;
uniform mat4 invProjMat;

uniform int debug = 0;

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
	vec3 viewNormal = texture(gBuffer1, inTexCoord).rgb;
	if (length(viewNormal) < 0.5) {
		// Normal not defined.
		outFragColor = vec4(0);
		return;
	}

	vec4 gb0 = texture(gBuffer0, inTexCoord);
	vec3 baseColor = gb0.rgb;
	float roughness = gb0.a;

	// Determine coordinates of fragment.
	float depth = texture(depthStencil, inTexCoord).r;
	vec3 fragPosition = ScreenPosToViewPos(inTexCoord, 0, invProjMat);
	vec3 viewPosition = ScreenPosToViewPos(inTexCoord, depth, invProjMat);
	vec3 directionFromView = normalize(viewPosition);
	vec3 worldPosition = (invViewMat * vec4(viewPosition, 1.0)).xyz;
	vec3 worldFragPosition = (invViewMat * vec4(fragPosition, 1.0)).xyz;

	vec3 worldNormal = invViewRotMat * viewNormal;

	// Trace.
	vec3 rayDir = normalize(worldPosition - worldFragPosition);
	vec3 rayReflectDir = reflect(rayDir, worldNormal);

	vec3 indirectSpecular = vec3(0);
	vec4 indirectDiffuse = vec4(0);

	vec4 directLight = texture(lastOutput, inTexCoord);

	for (int i = 0; i < maxReflections; i++) {
		// specular
		vec3 sampleDir = rayReflectDir;
		vec4 sampleColor = ConeTraceGrid(roughness, worldPosition, sampleDir, worldNormal);

		if (roughness == 0 && sampleColor.a >= 0) {
			worldPosition += sampleDir * (sampleColor.a - 0.5 * voxelSize);
			vec3 voxelPos = (worldPosition - voxelGridCenter) / voxelSize + VoxelGridSize * 0.5;
			GetVoxel(voxelPos, 0, baseColor, worldNormal, directLight.rgb, roughness);
			rayReflectDir = reflect(sampleDir, worldNormal);
		} else {
			indirectSpecular = sampleColor.rgb;
			break;
		}
	}

	for (float r = 0; r < diffuseAngles; r++) {
		for (float a = 0.3; a <= 0.9; a += 0.3) {
			// diffuse
			vec3 sampleDir = orientByNormal(r / diffuseAngles * 6.28, a, worldNormal);
			vec4 sampleColor = ConeTraceGridDiffuse(worldPosition, sampleDir, worldNormal);

			indirectDiffuse += sampleColor * cos(a);
		}
	}

	indirectDiffuse /= diffuseAngles;

	vec3 indirectLight = roughness * (indirectDiffuse.rgb * baseColor + directLight.rgb) + (1.0 - roughness) * indirectSpecular;

	if (debug == 1) { // combined
		indirectLight = roughness * indirectDiffuse.rgb * baseColor + (1.0 - roughness) * indirectSpecular;
		outFragColor = vec4(indirectLight, 1.0);
	} else if (debug == 2) { // diffuse
		outFragColor = vec4(indirectDiffuse.rgb, 1.0);
	} else if (debug == 3) { // specular
		outFragColor = vec4(indirectSpecular, 1.0);
	} else {
		outFragColor = vec4(indirectLight, 0.0);
	}
}
