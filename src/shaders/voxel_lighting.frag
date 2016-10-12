#version 430

##import lib/util
##import lib/lighting_util
##import voxel_shared

layout (binding = 0) uniform sampler2D lastOutput;
layout (binding = 1) uniform sampler2D gBuffer0;
layout (binding = 2) uniform sampler2D gBuffer1;
layout (binding = 3) uniform sampler2D depthStencil;
layout (binding = 4) uniform sampler3D voxelColor;
layout (binding = 5) uniform sampler3D voxelNormal;

layout (location = 0) in vec2 inTexCoord;
layout (location = 0) out vec4 outFragColor;

const float radius = 0.4;
const float power = 2.6;

uniform mat4 viewMat;
uniform mat4 invViewMat;
uniform mat3 invViewRotMat;
uniform mat4 projMat;
uniform mat4 invProjMat;

uniform int debug = 0;

const int diffuseSamples = 6;

// theta, phi
const vec2[diffuseSamples] diffuseAngles = vec2[](
	vec2(0, 0),
	vec2(0.8, 0),
	vec2(0.8, 1.26),
	vec2(0.8, 2.51),
	vec2(0.8, 3.77),
	vec2(0.8, 5.03)
);

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
		outFragColor = vec4(1);
		return;
	}

	vec4 gb0 = texture(gBuffer0, inTexCoord);
	float roughness = 1.0 - gb0.a;

	vec3 worldNormal = invViewRotMat * viewNormal;

	// Determine coordinates of fragment.
	float depth = texture(depthStencil, inTexCoord).r;
	vec3 fragPosition = ScreenPosToViewPos(inTexCoord, 0, invProjMat);
	vec3 viewPosition = ScreenPosToViewPos(inTexCoord, depth, invProjMat);
	vec3 directionFromView = normalize(viewPosition);
	vec3 worldPosition = (invViewMat * vec4(viewPosition, 1.0)).xyz;
	vec3 worldFragPosition = (invViewMat * vec4(fragPosition, 1.0)).xyz;

	// Trace.
	vec3 rayDir = normalize(worldPosition - worldFragPosition);
	vec3 rayReflectDir = reflect(rayDir, worldNormal);

	vec3 indirectSpecular = vec3(0);
	vec4 indirectDiffuse = vec4(0);
	vec3 voxelPosition = worldPosition - VoxelGridCenter;

	{
		// specular
		vec3 sampleDir = rayReflectDir;
		vec4 sampleColor = ConeTraceGrid(voxelColor, voxelNormal, roughness, voxelPosition, sampleDir, worldNormal);

		indirectSpecular = sampleColor.rgb;
	}

	for (int i = 0; i < diffuseSamples; i++) {
		// diffuse
		vec2 angle = diffuseAngles[i];
		vec3 sampleDir = orientByNormal(angle.y, angle.x, worldNormal);
		vec4 sampleColor = ConeTraceGrid(voxelColor, voxelNormal, 0.5, voxelPosition, sampleDir, worldNormal);

		indirectDiffuse += sampleColor;
	}

	indirectDiffuse /= float(diffuseSamples);
	indirectDiffuse.rgb *= 1.0 - indirectDiffuse.a;

	vec3 indirectLight = roughness * indirectDiffuse.rgb * gb0.rgb + (1.0 - roughness) * indirectSpecular;

	vec4 directLight = texture(lastOutput, inTexCoord);
	if (debug == 1) { // combined
		outFragColor = vec4(indirectLight, 1.0);
	} else if (debug == 2) { // diffuse
		outFragColor = vec4(indirectDiffuse.rgb, 1.0);
	} else if (debug == 3) { // specular
		outFragColor = vec4(indirectSpecular, 1.0);
	} else if (debug == 4) { // AO
		outFragColor = vec4(vec3(1.0 - indirectDiffuse.a), 1.0);
	} else {
		outFragColor = directLight + vec4(indirectLight, 0.0);
	}
}
