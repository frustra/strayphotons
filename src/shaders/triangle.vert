#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inColor;

uniform mat4 projMatrix;
uniform mat4 modelMatrix;
uniform mat4 viewMatrix;

layout (location = 0) out vec3 outColor;

out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

void main()
{
	outColor = inColor;
	gl_Position = projMatrix * viewMatrix * modelMatrix * vec4(inPos, 1.0);
}
