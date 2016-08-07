#version 430

##import lib/util

layout (binding = 0) uniform sampler2D gBuffer0;
layout (binding = 1) uniform sampler2D gBuffer1;
layout (binding = 2) uniform sampler2D depthStencil;
layout (binding = 3) uniform usampler3D voxelColorRG;
layout (binding = 4) uniform usampler3D voxelColorBA;

layout (location = 0) in vec2 inTexCoord;
layout (location = 0) out vec4 outFragColor;

uniform int mode;
uniform mat4 invProjMat;

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
		uint data = texture(voxelColorRG, vec3(inTexCoord, 3.0/256)).r;
		float red = float((data & 0xFFFF0000) >> 16) / 256.0;
		float green = float(data & 0xFFFF) / 256.0;
		data = texture(voxelColorBA, vec3(inTexCoord, 3.0/256)).r;
		float blue = float((data & 0xFFFF0000) >> 16) / 256.0;
		uint count = data & 0xFFFF;
		outFragColor.rgb = vec3(red, green, blue) / float(count);
	}
}