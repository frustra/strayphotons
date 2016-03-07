#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inColor;

layout (binding = 0) uniform UBO
{
	mat4 projMatrix;
	mat4 modelMatrix;
	mat4 viewMatrix;
} ubo;

layout (location = 0) out vec3 outColor;

void main()
{
	outColor = inColor;
	gl_Position = ubo.projMatrix * ubo.viewMatrix * ubo.modelMatrix * vec4(inPos, 1.0);
}

