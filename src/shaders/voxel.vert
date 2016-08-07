#version 430

##import lib/vertex_base

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

layout (location = 0) out vec3 outViewPos;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec2 outTexCoord;

void main()
{
	vec4 tmp = view * model * vec4(inPos, 1.0);
	outViewPos = tmp.xyz / tmp.w;
	gl_Position = projection * tmp;
	outNormal = mat3(view * model) * inNormal;
	outTexCoord = inTexCoord;
}
