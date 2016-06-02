#version 430

##import lib/util

layout (binding = 0) uniform sampler2D tex;

layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 inTexCoord;

layout (location = 0) out vec4 outFragColor;

void main()
{
	outFragColor = texture(tex, inTexCoord) * vec4(inColor, 1.0);
}
