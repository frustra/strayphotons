#version 430

##import lib/vertex_base

in gl_PerVertex
{
    vec4 gl_Position;
    float gl_PointSize;
    float gl_ClipDistance[];
} gl_in[];

layout (triangles) in;
layout (triangle_strip, max_vertices = 27) out;

layout (location = 0) in vec3 inViewPos[];
layout (location = 1) in vec3 inNormal[];
layout (location = 2) in vec2 inTexCoord[];

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outTangent;
layout (location = 2) out vec3 outBitangent;
layout (location = 3) out vec2 outTexCoord;
layout (location = 4) out vec3 outViewPos;

void main()
{
	vec3 dPos1 = inViewPos[1] - inViewPos[0];
	vec3 dPos2 = inViewPos[2] - inViewPos[0];

	vec2 dUV1 = inTexCoord[1] - inTexCoord[0];
	vec2 dUV2 = inTexCoord[2] - inTexCoord[0];

	float r = 1.0 / (dUV1.x * dUV2.y - dUV1.y * dUV2.x);
	outTangent = -(dPos1 * dUV2.y - dPos2 * dUV1.y)*r;
	outBitangent = -(dPos2 * dUV1.x - dPos1 * dUV2.x)*r;

	for (int i = 0; i < 3; i++) {
		outNormal = inNormal[i];
		outTexCoord = inTexCoord[i];
		outViewPos = inViewPos[i];

		gl_Position = gl_in[i].gl_Position;
    	EmitVertex();
	}

    EndPrimitive();
}
