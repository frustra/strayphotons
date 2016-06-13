#version 430

##import lib/util

layout (binding = 0) uniform sampler2D tex;
layout (location = 0) in vec2 inTexCoord;
layout (location = 0) out vec4 outFragColor;

void main()
{
	vec4 frag = texture(tex, inTexCoord);
	outFragColor = vec4(FastLinearToSRGB(frag.rgb), frag.a);
}