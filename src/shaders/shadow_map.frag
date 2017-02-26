#version 430

##import lib/util

layout (location = 0) in vec3 inViewPos;

##import lib/types_common
##import lib/mirror_shadow_common

uniform vec2 clip;
uniform int drawLightId;
uniform int drawMirrorId;

layout (location = 0) out vec4 gBuffer0;

void main()
{
	if (drawMirrorId >= 0) {
		uint mask = 1 << uint(drawMirrorId);
		uint prevValue = atomicOr(mirrorData.mask[drawLightId], mask);
		if ((prevValue & mask) == 0) {
			uint index = atomicAdd(mirrorData.count[0], 1);
			mirrorData.list[index] = PackLightAndMirror(drawLightId, drawMirrorId);
			mirrorData.sourceLight[index] = drawLightId;
		}
	}

	gBuffer0.r = LinearDepth(inViewPos, clip);
}
