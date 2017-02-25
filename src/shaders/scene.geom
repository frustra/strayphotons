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

layout (location = 0) in vec3 inNormal[];
layout (location = 1) in vec2 inTexCoord[];

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outTangent;
layout (location = 2) out vec3 outBitangent;
layout (location = 3) out vec2 outTexCoord;
layout (location = 4) out vec3 outViewPos;
layout (location = 5) flat out int outMirrorIndex;

##import lib/types_common
##import lib/mirror_scene_common

uniform bool renderMirrors = false;
uniform mat4 view;
uniform mat4 projection;

void main() {
	vec3 viewPos[3] = vec3[](
		vec3(view * gl_in[0].gl_Position),
		vec3(view * gl_in[1].gl_Position),
		vec3(view * gl_in[2].gl_Position)
	);

	vec3 dPos1 = viewPos[1] - viewPos[0];
	vec3 dPos2 = viewPos[2] - viewPos[0];

	vec2 dUV1 = inTexCoord[1] - inTexCoord[0];
	vec2 dUV2 = inTexCoord[2] - inTexCoord[0];

	float r = 1.0 / (dUV1.x * dUV2.y - dUV1.y * dUV2.x);
	outTangent = -(dPos1 * dUV2.y - dPos2 * dUV1.y)*r;
	outBitangent = -(dPos2 * dUV1.x - dPos1 * dUV2.x)*r;

	mat3 normalMat = mat3(view);

	if (!renderMirrors) {
		outMirrorIndex = -1;
		gl_ClipDistance[0] = 1.0;

		for (int i = 0; i < 3; i++) {
			outNormal = normalMat * inNormal[i];
			outTexCoord = inTexCoord[i];
			outViewPos = viewPos[i];
			gl_Position = projection * vec4(outViewPos, 1);
			EmitVertex();
		}

		EndPrimitive();
	} else {
		for (int index = mirrorSData.count[1]; index < mirrorSData.count[0]; index++) {
			outMirrorIndex = index;
			mat4 view2 = view * mirrorSData.viewMat[index];

			for (int i = 0; i < 3; i++) {
				outNormal = normalMat * inNormal[i];
				outTexCoord = inTexCoord[i];
				outViewPos = viewPos[i];
				gl_ClipDistance[0] = dot(mirrorSData.clipPlane[index], gl_in[i].gl_Position);
				gl_Position = projection * view2 * gl_in[i].gl_Position;
				EmitVertex();
			}

			EndPrimitive();
			break;
		}
	}
}
