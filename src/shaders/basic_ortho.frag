#version 430

layout(binding = 0) uniform sampler2D tex;

layout (location = 0) in vec2 inTexCoord;
layout (location = 1) in vec4 inColor;

layout (location = 0) out vec4 outFragColor;

void main() {
	outFragColor = inColor * texture(tex, inTexCoord);
}