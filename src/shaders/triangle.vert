#version 430

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inTex;

uniform mat4 projMatrix;
uniform mat4 modelMatrix;
uniform mat4 viewMatrix;

layout (location = 0) out vec3 outColor;
layout (location = 1) out vec2 outTex;

out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

void main()
{
	outColor = inColor;
	outTex = inTex;
	gl_Position = projMatrix * viewMatrix * modelMatrix * vec4(inPos, 1.0);
}
