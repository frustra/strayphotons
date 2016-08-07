#version 430

##import lib/vertex_base

in gl_PerVertex
{
    vec4 gl_Position;
    float gl_PointSize;
    float gl_ClipDistance[];
} gl_in[];

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

layout (location = 0) in vec3 inViewPos[];
layout (location = 1) in vec3 inNormal[];
layout (location = 2) in vec2 inTexCoord[];

layout (location = 0) out vec3 outViewPos;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec2 outTexCoord;

void main()
{
	for (int i = 0; i < 3; i++) {
		outViewPos = inViewPos[i];
		outNormal = inNormal[i];
		outTexCoord = inTexCoord[i];

		gl_Position = gl_in[i].gl_Position;
    	EmitVertex();
	}

    EndPrimitive();
}
