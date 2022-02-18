#version 430

layout (early_fragment_tests) in; // Force stencil testing before shader invocation.

##import lib/util

layout (location = 0) in vec3 inViewPos;

##import lib/types_common
##import mirror_shadow_common

uniform vec2 clip;
uniform int drawLightId;
uniform int drawMirrorId;

layout (location = 0) out vec4 gBuffer0;

void main()
{
	if (drawMirrorId >= 0) {
		mirrorData.mask[drawLightId][drawMirrorId] = 1;
	}

	gBuffer0.r = LinearDepth(inViewPos, clip);
}
