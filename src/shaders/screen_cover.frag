#version 430

##import lib/post_inputs

layout (location = 0) in vec2 inTexCoord;
layout (location = 0) out vec4 outFragColor;

void main()
{
	outFragColor = texture(colorTexture, inTexCoord);
}