#version 430

##import lib/util

layout (binding = 0) uniform sampler2D baseColorTex;
layout (binding = 1) uniform sampler2D roughnessTex;

layout (binding = 0, r32ui) uniform uimage3D voxelColorRG;
layout (binding = 1, r32ui) uniform uimage3D voxelColorBA;

layout (location = 0) in vec3 inViewPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexCoord;

in vec4 gl_FragCoord;

void main()
{
	vec4 diffuseColor = texture(baseColorTex, inTexCoord);
	if (diffuseColor.a < 0.5) discard;

	uint rg = (uint(diffuseColor.r * 0xFF) << 16) + uint(diffuseColor.g * 0xFF);
	uint ba = (uint(diffuseColor.b * 0xFF) << 16) + 1;

	imageAtomicAdd(voxelColorRG, ivec3(gl_FragCoord.xy, -inViewPos.z), rg);
	imageAtomicAdd(voxelColorBA, ivec3(gl_FragCoord.xy, -inViewPos.z), ba);
	// imageAtomicAdd(voxelGrid, ivec3(gl_FragCoord.xy, 0), 1);
}
