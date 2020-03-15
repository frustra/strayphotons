#version 430

##import lib/vertex_base

layout (location = 0) in vec3 inPos;

uniform mat4 model;
uniform mat4 primitive;
uniform mat4 view;
uniform mat4 projection;

layout (location = 0) out vec3 outViewPos;

void main()
{
	outViewPos = vec3(view * model * primitive * vec4(inPos, 1.0));
	gl_Position = projection * vec4(outViewPos, 1.0);
}
