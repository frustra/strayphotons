#version 430

layout (binding = 0) uniform sampler2D gBuffer0;
layout (binding = 1) uniform sampler2D gBuffer1;
layout (binding = 2) uniform sampler2D depthStencil;
layout (binding = 3) uniform sampler3D voxelColor;
layout (binding = 4) uniform sampler3D voxelAlpha;
layout (binding = 5) uniform sampler3D voxelRadiance;

layout (location = 0) in vec2 inTexCoord;
layout (location = 0) out vec4 outFragColor;

#define INCLUDE_VOXEL_TRACE

uniform float voxelSize = 0.1;
uniform vec3 voxelGridCenter = vec3(0);

##import lib/util
##import voxel_shared

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
		vec3 sampleRadiance;
		float alpha = TraceVoxelGrid(mipLevel, rayPos.xyz, rayDir.xyz, outFragColor.rgb, sampleRadiance);
	} else if (mode == 6) {
		vec4 rayPos = invViewMat * vec4(ScreenPosToViewPos(inTexCoord, 0, invProjMat), 1);
		vec4 rayDir = normalize(rayPos - (invViewMat * vec4(0, 0, 0, 1)));
		vec3 sampleColor;
		float alpha = TraceVoxelGrid(mipLevel, rayPos.xyz, rayDir.xyz, sampleColor, outFragColor.rgb);
	} else if (mode == 7) {
		vec4 rayPos = invViewMat * vec4(ScreenPosToViewPos(inTexCoord, 0, invProjMat), 1);
		vec4 rayDir = normalize(rayPos - (invViewMat * vec4(0, 0, 0, 1)));
		vec3 sampleColor;
		vec3 sampleRadiance;
		float alpha = TraceVoxelGrid(mipLevel, rayPos.xyz, rayDir.xyz, sampleColor, sampleRadiance);
		outFragColor.rgb = vec3(alpha);
	} else if (mode == 8) {
		vec4 rayPos = invViewMat * vec4(ScreenPosToViewPos(inTexCoord, 0, invProjMat), 1);
		vec4 rayDir = normalize(rayPos - (invViewMat * vec4(0, 0, 0, 1)));

		outFragColor.rgb = ConeTraceGrid(float(mipLevel) / 50.0, rayPos.xyz, rayDir.xyz, rayDir.xyz).rgb;
	}
}