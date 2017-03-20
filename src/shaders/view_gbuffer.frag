#version 430

layout (binding = 0) uniform sampler2D gBuffer0;
layout (binding = 1) uniform sampler2D gBuffer1;
layout (binding = 2) uniform sampler2D gBuffer2;
layout (binding = 3) uniform sampler2D depthStencil;
layout (binding = 4) uniform sampler3D voxelRadiance;
layout (binding = 5) uniform sampler3D voxelNormal;

layout (location = 0) in vec2 inTexCoord;
layout (location = 0) out vec4 outFragColor;

uniform float voxelSize = 0.1;
uniform vec3 voxelGridCenter = vec3(0);

##import lib/util
##import voxel_shared
##import voxel_trace_shared2

uniform int mode;
uniform int source;
uniform int mipLevel;
uniform mat4 invProjMat;
uniform mat4 invViewMat;

void main()
{
	vec4 rayPos = invViewMat * vec4(ScreenPosToViewPos(inTexCoord, 0, invProjMat), 1);
	vec4 rayDir = normalize(rayPos - (invViewMat * vec4(0, 0, 0, 1)));

	if (source == 0) { // gBuffer source
		if (mode == 1) { // Base color
			outFragColor.rgb = texture(gBuffer0, inTexCoord).rgb;
		} else if (mode == 2) { // Normal
			outFragColor.rgb = DecodeNormal(texture(gBuffer1, inTexCoord).rg);
		} else if (mode == 3) { // Depth
			float depth = texture(depthStencil, inTexCoord).r;
			vec3 position = ScreenPosToViewPos(inTexCoord, depth, invProjMat);
			outFragColor.rgb = vec3(-position.z / 32.0);
		} else if (mode == 4) { // Roughness
			outFragColor.rgb = vec3(texture(gBuffer0, inTexCoord).a);
		} else if (mode == 5) { // Metallic
			outFragColor.rgb = vec3(texture(gBuffer2, inTexCoord).a);
		} else if (mode == 6) { // Position
			outFragColor.rgb = vec3(texture(gBuffer2, inTexCoord).rgb * 0.1 + 0.5);
		} else if (mode == 7) { // Face normal
			outFragColor.rgb = DecodeNormal(texture(gBuffer1, inTexCoord).ba);
		}
	} else if (source == 1) { // Voxel source
		vec3 sampleRadiance, sampleNormal;
		float alpha = TraceVoxelGrid(mipLevel, rayPos.xyz, rayDir.xyz, sampleRadiance, sampleNormal);

		if (mode == 1) { // Radiance
			outFragColor.rgb = sampleRadiance;
		} else if (mode == 2) { // Normal
			outFragColor.rgb = sampleNormal;
		}
	} else if (source == 2) { // Cone trace source
		vec4 sampleValue = ConeTraceGrid(float(mipLevel) / 50.0, rayPos.xyz, rayDir.xyz, rayDir.xyz, gl_FragCoord.xy);
		if (mode == 1) { // Radiance
			outFragColor.rgb = sampleValue.rgb;
		} else if (mode == 2) { // Position
			outFragColor.rgb = rayPos.xyz + rayDir.xyz * sampleValue.a;
		}
	}
}
