#version 430

layout (binding = 0) uniform usampler2D tex;
layout (location = 0) in vec2 inTexCoord;
layout (location = 0) out uint outFragColor;

void main()
{
	outFragColor = texture(tex, inTexCoord).r;
}
