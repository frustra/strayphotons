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

layout (location = 0) in vec3 inNormal[];
layout (location = 1) in vec2 inTexCoord[];

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outTexCoord;
layout (location = 2) out int outDirection;

const mat4[3] axisRotation = mat4[](
	mat4(mat3(0, 0, -1, 0, 1, 0, 1, 0, 0)),
	mat4(mat3(1, 0, 0, 0, 0, -1, 0, 1, 0)),
	mat4(1.0)
);

void main()
{
	vec3 edge1 = vec3(gl_in[1].gl_Position - gl_in[0].gl_Position);
	vec3 edge2 = vec3(gl_in[2].gl_Position - gl_in[0].gl_Position);
	vec3 realNormal = cross(edge1, edge2);
	vec3 absNormal = abs(realNormal);
	ivec3 signNormal = ivec3(sign(realNormal));
	if (absNormal.x > absNormal.y) {
		if (absNormal.x > absNormal.z) {
			outDirection = 1 * signNormal.x;
		} else {
			outDirection = 3 * signNormal.z;
		}
	} else if (absNormal.y > absNormal.z) {
		outDirection = 2 * signNormal.y;
	} else {
		outDirection = 3 * signNormal.z;
	}
	mat4 rotation = axisRotation[abs(outDirection)-1];

	for (int i = 0; i < 3; i++) {
		outNormal = inNormal[i];
		outTexCoord = inTexCoord[i];

		gl_Position = rotation * gl_in[i].gl_Position;
    	EmitVertex();
	}

    EndPrimitive();
}
