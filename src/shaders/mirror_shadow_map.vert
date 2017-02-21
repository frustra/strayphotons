#version 430

##import lib/vertex_base

layout (location = 0) in vec3 inPos;

uniform mat4 model;

void main()
{
	gl_Position = model * vec4(inPos, 1.0);
}
