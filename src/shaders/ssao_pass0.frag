#version 430

layout (binding = 0) uniform sampler2D gBuffer0;
layout (binding = 1) uniform sampler2D gBuffer1;
layout (binding = 2) uniform sampler2D depthStencil;

layout (location = 0) in vec2 inTexCoord;
layout (location = 0) out vec4 outFragColor;

void main()
{
	outFragColor.rgb = texture(gBuffer1, inTexCoord).rgb;
}