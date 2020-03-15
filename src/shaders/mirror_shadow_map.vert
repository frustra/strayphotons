#version 430

##import lib/vertex_base

layout (location = 0) in vec3 inPos;

uniform mat4 model;
uniform mat4 primitive;

void main()
{
	gl_Position = model * primitive * vec4(inPos, 1.0);
}
