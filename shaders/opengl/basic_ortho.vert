#version 430

##import lib/vertex_base

uniform mat4 projMat;

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec4 inColor;

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out vec4 outColor;

void main() {
	outTexCoord = inTexCoord;
	outColor = inColor;
	gl_Position = projMat * vec4(inPos, 0.0, 1.0);
}
