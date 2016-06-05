#version 430

##import lib/vertex_base
##import lib/util

layout(location = 0) in vec3 position;
layout(location = 2) in vec2 texCoord;
layout (location = 0) out vec2 outTexCoord;
layout (location = 1) noperspective out vec3 outViewRay;

void main()
{
	gl_Position = vec4(position, 1);
	outTexCoord = texCoord;
	outViewRay = FarPlaneRay(position.xy);
}