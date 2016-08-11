#version 430

##import lib/util
##import voxel_shared

layout (binding = 0) uniform sampler2D baseColorTex;
layout (binding = 1) uniform sampler2D roughnessTex;

layout (binding = 0, r32ui) uniform uimage3D voxelFragList;
layout (binding = 1, r32ui) uniform uimage3D voxelColor;
layout (binding = 2, r32ui) uniform uimage3D voxelNormal;

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inTexCoord;
layout (location = 2) in flat int inDirection;

in vec4 gl_FragCoord;

void main()
{
	vec4 diffuseColor = texture(baseColorTex, inTexCoord);
	if (diffuseColor.a < 0.5) discard;

	float roughness = texture(roughnessTex, inTexCoord).r;

	vec3 position = vec3(gl_FragCoord.xy, gl_FragCoord.z * VoxelGridSize);
	position = AxisSwapReverse[abs(inDirection)-1] * (position - VoxelGridSize / 2);
	position += VoxelGridSize / 2;

	uint rg = (uint(diffuseColor.r * 0xFF) << 16) + uint(diffuseColor.g * 0xFF);
	uint ba = (uint(diffuseColor.b * 0xFF) << 16) + 1;
	imageAtomicAdd(voxelColor, ivec3(floor(position.x) * 2, position.yz), rg);
	imageAtomicAdd(voxelColor, ivec3(floor(position.x) * 2 + 1, position.yz), ba);

	uint xy = (uint(inNormal.x * 0xFF) << 16) + uint(inNormal.y * 0xFF);
	uint zr = (uint(inNormal.z * 0xFF) << 16) + uint(roughness * 0xFF);
	imageAtomicAdd(voxelNormal, ivec3(floor(position.x) * 2, position.yz), xy);
	imageAtomicAdd(voxelNormal, ivec3(floor(position.x) * 2 + 1, position.yz), zr);
}
