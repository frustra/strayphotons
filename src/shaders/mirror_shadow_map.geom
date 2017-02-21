#version 430

##import lib/vertex_base

in gl_PerVertex
{
    vec4 gl_Position;
    float gl_PointSize;
    float gl_ClipDistance[];
} gl_in[];

##import lib/types_common
##import lib/mirror_common

layout (triangles) in;
layout (triangle_strip, max_vertices = 60) out; // MAX_MIRRORS * MAX_LIGHTS * 3

layout (location = 0) out vec3 outViewPos;

void main()
{
	for (int mirror = 0; mirror < mirrorData.count; mirror++) {
		gl_Layer = mirror;

		for (int i = 0; i < 3; i++) {
			outViewPos = vec3(mirrorData.viewMat[mirror] * gl_in[i].gl_Position);
			gl_Position = mirrorData.projMat[mirror] * vec4(outViewPos, 1.0);
	    	EmitVertex();
		}

	    EndPrimitive();
	}
}
