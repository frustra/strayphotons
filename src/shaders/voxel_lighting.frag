#version 430

##import lib/util
##import lib/lighting_util
##import voxel_shared

layout (binding = 0) uniform sampler2D lastOutput;
layout (binding = 1) uniform sampler2D gBuffer1;
layout (binding = 2) uniform sampler2D depthStencil;
layout (binding = 3) uniform sampler3D voxelColor;
layout (binding = 4) uniform sampler3D voxelNormal;

layout (location = 0) in vec2 inTexCoord;
layout (location = 0) out vec4 outFragColor;

const float radius = 0.4;
const float power = 2.6;

uniform mat4 viewMat;
uniform mat4 invViewMat;
uniform mat3 invViewRotMat;
uniform mat4 projMat;
uniform mat4 invProjMat;

void main()
{
	// Determine normal of surface at this fragment.
	vec3 viewNormal = texture(gBuffer1, inTexCoord).rgb;
	if (length(viewNormal) < 0.5) {
		// Normal not defined.
		outFragColor = vec4(1);
		return;
	}

	vec3 worldNormal = invViewRotMat * viewNormal;

	// Determine coordinates of fragment.
	float depth = texture(depthStencil, inTexCoord).r;
	vec3 fragPosition = ScreenPosToViewPos(inTexCoord, 0, invProjMat);
	vec3 viewPosition = ScreenPosToViewPos(inTexCoord, depth, invProjMat);
	vec3 directionFromView = normalize(viewPosition);
	vec3 worldPosition = (invViewMat * vec4(viewPosition, 1.0)).xyz;
	vec3 worldFragPosition = (invViewMat * vec4(fragPosition, 1.0)).xyz;

	// Offset to avoid self-intersection.
	// TODO(pushrax): care about anisotropic voxels
	worldPosition += worldNormal * VoxelSize * 1.74;

	// Trace.
	vec3 rayDir = normalize(worldPosition - worldFragPosition);
	vec3 rayReflectDir = reflect(rayDir, worldNormal);

	vec3 indirectLight;

	//for (int i = 0; i < 1; i++) {
		vec3 sampleDir = rayReflectDir;
		vec3 sampleColor, sampleNormal;
		TraceVoxelGrid(voxelColor, voxelNormal, 0, worldPosition - VoxelGridCenter, sampleDir, sampleColor, sampleNormal);

		indirectLight += sampleColor;
	//}

	indirectLight /= 1.0;

	vec4 directLight = texture(lastOutput, inTexCoord);
	outFragColor = directLight + vec4(indirectLight, 0.0);
}
