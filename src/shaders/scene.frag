#version 430

##import lib/util

layout (binding = 0) uniform sampler2D baseColorTex;

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inTexCoord;

layout (location = 0) out vec4 gBuffer0;
layout (location = 1) out vec4 gBuffer1;

void main()
{
	vec4 baseColor = texture(baseColorTex, inTexCoord);
	if (baseColor.a < 0.5) discard;
	vec3 viewNormal = normalize(inNormal);

	gBuffer0.rgb = baseColor.rgb;
	gBuffer0.a = 1.0;
	gBuffer1.rgb = viewNormal;
	gBuffer1.a = 1.0;
}
