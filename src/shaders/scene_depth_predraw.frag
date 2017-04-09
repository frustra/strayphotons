#version 430

##import lib/util
##import lib/types_common
##import lib/mirror_scene_common

layout (binding = 4) uniform usampler2D inMirrorIndexStencil;

layout (location = 5) flat in int inMirrorIndex;

void main() {
	// Discard fragments that are in an area not meant for this mirror.
	vec2 screenTexCoord = gl_FragCoord.xy / textureSize(inMirrorIndexStencil, 0).xy;
	uint mirrorIndexStencil = texture(inMirrorIndexStencil, screenTexCoord).x;

	// Value is offset by 1.
	if (mirrorIndexStencil != mirrorSData.list[inMirrorIndex] + 1) {
		discard;
	}
}
