#version 430

##import lib/util

layout (location = 0) in vec3 inViewPos;

uniform vec2 clip;

layout (location = 0) out vec4 gBuffer0;

void main()
{
	gBuffer0.r = (length(inViewPos) - clip.x) / (clip.y - clip.x);
}
