#version 430

##import lib/util

layout (location = 0) in vec3 inViewPos;
layout (location = 1) in flat int mirrorId; // TODO(xthexder): use this

uniform vec2 clip;

layout (location = 0) out vec4 gBuffer0;

void main()
{
	gBuffer0.r = LinearDepth(inViewPos, clip);
}
