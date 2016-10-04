#version 430

##import lib/util
##import voxel_shared

layout (binding = 0) uniform sampler2D gBuffer0;
layout (binding = 1) uniform sampler2D gBuffer1;
layout (binding = 2) uniform sampler2D depthStencil;
layout (binding = 3) uniform sampler3D voxelColor;
layout (binding = 4) uniform sampler3D voxelNormal;

layout (location = 0) in vec2 inTexCoord;
layout (location = 0) out vec4 outFragColor;

uniform int mode;
uniform int mipLevel;
uniform mat4 invProjMat;
uniform mat4 invViewMat;

void main()
{
	if (mode == 1) {
		outFragColor.rgb = texture(gBuffer0, inTexCoord).rgb;
	} else if (mode == 2) {
		outFragColor.rgb = texture(gBuffer1, inTexCoord).rgb * vec3(0.5, 0.5, 1.0) + vec3(0.5, 0.5, 0.0);
	} else if (mode == 3) {
		float depth = texture(depthStencil, inTexCoord).r;
		vec3 position = ScreenPosToViewPos(inTexCoord, depth, invProjMat);
		outFragColor.rgb = vec3(-position.z / 32.0);
	} else if (mode == 4) {
		outFragColor.rgb = vec3(texture(gBuffer0, inTexCoord).a);
	} else if (mode == 5) {
		vec4 rayPos = invViewMat * vec4(ScreenPosToViewPos(inTexCoord, 0, invProjMat), 1);
		vec4 rayDir = normalize(rayPos - (invViewMat * vec4(0, 0, 0, 1)));
		TraceVoxelGrid(voxelColor, mipLevel, rayPos.xyz - VoxelGridCenter, rayDir.xyz, outFragColor.rgb);
	}
}