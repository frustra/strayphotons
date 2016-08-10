#version 430

##import lib/util

layout (binding = 0) uniform sampler2D gBuffer0;
layout (binding = 1) uniform sampler2D gBuffer1;
layout (binding = 2) uniform sampler2D depthStencil;
layout (binding = 3) uniform usampler3D voxelColor;

layout (location = 0) in vec2 inTexCoord;
layout (location = 0) out vec4 outFragColor;

uniform int mode;
uniform mat4 invProjMat;
uniform mat4 invViewMat;

const float VoxelGridSize = 256;
const float VoxelSize = 0.0453;

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
		vec4 worldPos = invViewMat * vec4(ScreenPosToViewPos(inTexCoord, 0, invProjMat), 1);
		vec4 dir = normalize(worldPos - (invViewMat * vec4(0, 0, 0, 1)));

		for (int i = 0; i < 10000; i++) {
			ivec3 position = ivec3(worldPos.xyz * 0.5 / VoxelSize + VoxelGridSize / 2) * ivec3(2, 1, 1);
			position.z = position.z % int(VoxelGridSize);
			if (i > 512) {
				if (position.x < 0 || position.x >= VoxelGridSize * 2 || position.y < 0 || position.y >= VoxelGridSize) break;
			}
			uint data = texelFetch(voxelColor, position, 0).r;
			uint count = data & 0xFFFF;
			if (count > 0) {
				float blue = float((data & 0xFFFF0000) >> 16) / 256.0;
				data = texelFetch(voxelColor, position + ivec3(1, 0, 0), 0).r;
				float red = float((data & 0xFFFF0000) >> 16) / 256.0;
				float green = float(data & 0xFFFF) / 256.0;
				outFragColor.rgb = vec3(red, green, blue) / float(count);
				return;
			}
			worldPos += dir * VoxelSize/2;
		}
		outFragColor.rgb = vec3(0);
	}
}