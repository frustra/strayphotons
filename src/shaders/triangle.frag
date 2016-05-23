#version 430

##import lib/util

layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 inTex;
layout (binding = 0) uniform sampler2D tex;

layout (location = 0) out vec4 outFragColor;

void main()
{
	outFragColor = texture(tex, inTex);
}
