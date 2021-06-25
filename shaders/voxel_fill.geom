#version 430

##import lib/vertex_base
##import voxel_shared

in gl_PerVertex
{
    vec4 gl_Position;
    float gl_PointSize;
    float gl_ClipDistance[];
} gl_in[];

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

layout (location = 0) in vec2 inTexCoord[];

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outTexCoord;
layout (location = 2) out int outDirection;

void main()
{
	vec3 edge1 = vec3(gl_in[1].gl_Position - gl_in[0].gl_Position);
	vec3 edge2 = vec3(gl_in[2].gl_Position - gl_in[0].gl_Position);
	vec3 edge3 = vec3(gl_in[2].gl_Position - gl_in[1].gl_Position);
	vec3 realNormal = normalize(cross(edge1, edge2));

	outDirection = DominantAxis(realNormal);
	mat4 rotation = AxisSwapForward[abs(outDirection)-1];

	for (int i = 0; i < 3; i++) {
		outNormal = realNormal;
		outTexCoord = inTexCoord[i];

		gl_Position = rotation * gl_in[i].gl_Position;
		EmitVertex();
	}

    EndPrimitive();
}
