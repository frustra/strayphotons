#version 430

##import lib/util

layout (location = 0) in vec3 inViewPos;

uniform vec2 clip;

layout (location = 0) out vec4 gBuffer0;

void main()
{
	float depth = (length(inViewPos) - clip.x) / (clip.y - clip.x);

	// Variance shadow map component
	float dx = dFdx(depth);
	float dy = dFdy(depth);
	gBuffer0.rg = vec2(depth, depth*depth);// + 0.25*(dx*dx + dy*dy));
}
