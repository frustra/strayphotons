#version 430

##import lib/util

layout (location = 0) in vec3 inViewPos;

uniform vec2 clip;

layout (location = 0) out vec4 gBuffer0;

void main()
{
	vec2 depth = WarpDepth(inViewPos, clip, 0);
	gBuffer0 = vec4(depth, depth*depth);
}
