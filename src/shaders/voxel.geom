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

layout (location = 0) in vec3 inNormal[];
layout (location = 1) in vec2 inTexCoord[];

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outTexCoord;
layout (location = 2) out int outDirection;

void main()
{
	vec3 edge1 = vec3(gl_in[1].gl_Position - gl_in[0].gl_Position);
	vec3 edge2 = vec3(gl_in[2].gl_Position - gl_in[0].gl_Position);
	vec3 edge3 = vec3(gl_in[2].gl_Position - gl_in[1].gl_Position);
	vec3 realNormal = cross(edge1, edge2);
	vec3 absNormal = abs(realNormal);

	bvec3 mask = greaterThanEqual(absNormal.xyz, max(absNormal.yzx, absNormal.zxy));
	outDirection = int(dot(vec3(mask) * sign(realNormal), vec3(1, 2, 3)));

	mat4 rotation = AxisSwapForward[abs(outDirection)-1];

	for (int i = 0; i < 3; i++) {
		outNormal = inNormal[i];
		outTexCoord = inTexCoord[i];

		gl_Position = rotation * gl_in[i].gl_Position;
		EmitVertex();
	}

    EndPrimitive();
}
