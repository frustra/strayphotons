#version 430

##import lib/vertex_base

in gl_PerVertex
{
    vec4 gl_Position;
    float gl_PointSize;
    float gl_ClipDistance[];
} gl_in[];

##import lib/types_common
##import lib/mirror_shadow_common

layout (triangles) in;
layout (triangle_strip, max_vertices = 27) out; // MAX_MIRRORS * MAX_LIGHTS * 3

layout (location = 0) out vec3 outViewPos;

void main()
{
	for (int index = mirrorData.count[1]; index < mirrorData.count[0]; index++) {
		gl_Layer = index;

		for (int i = 0; i < 3; i++) {
			vec4 viewPos = mirrorData.viewMat[index] * gl_in[i].gl_Position;
			gl_Position = mirrorData.projMat[index] * viewPos;
			outViewPos = viewPos.xyz;
	    	EmitVertex();
		}

	    EndPrimitive();
	}
}
