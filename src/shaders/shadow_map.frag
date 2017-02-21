#version 430

##import lib/util

layout (location = 0) in vec3 inViewPos;

##import lib/types_common
##import lib/mirror_common

uniform vec2 clip;
uniform int lightId;
uniform int mirrorId;

layout (location = 0) out vec4 gBuffer0;

void main()
{
	if (mirrorId >= 0) {
		uint mask = 1 << mirrorId;
		uint prevValue = atomicOr(mirrorData.mask[lightId], mask);
		if ((prevValue & mask) == 0) {
			uint index = atomicAdd(mirrorData.count, 1);
			mirrorData.list[index] = lightId << 16 + mirrorId;
		}
	}

	gBuffer0.r = LinearDepth(inViewPos, clip);
}
