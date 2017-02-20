#version 430

##import lib/vertex_base

layout (location = 0) in vec3 inPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform int mirrorId;

layout (location = 0) out vec3 outViewPos;
layout (location = 1) out int outMirrorId;

void main()
{
	outMirrorId = mirrorId;
	outViewPos = vec3(view * model * vec4(inPos, 1.0));
	gl_Position = projection * vec4(outViewPos, 1.0);
}
