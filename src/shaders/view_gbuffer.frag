#version 430

##import lib/util
##import raytrace/intersection

layout (binding = 0) uniform sampler2D gBuffer0;
layout (binding = 1) uniform sampler2D gBuffer1;
layout (binding = 2) uniform sampler2D depthStencil;
layout (binding = 3) uniform usampler3D voxelColor;

layout (location = 0) in vec2 inTexCoord;
layout (location = 0) out vec4 outFragColor;

uniform int mode;
uniform mat4 invProjMat;
uniform mat4 invViewMat;

const float VoxelGridSize = 256;
const float VoxelSize = 0.0453;

bool UnpackVoxel(usampler3D voxels, ivec3 position, out vec3 hitColor)
{
	ivec3 index = position * ivec3(2, 1, 1);
	uint data = texelFetch(voxels, index, 0).r;
	uint count = data & 0xFFFF;

	if (count > 0) {
		float blue = float((data & 0xFFFF0000) >> 16) / 256.0;

		data = texelFetch(voxels, index + ivec3(1, 0, 0), 0).r;
		float red = float((data & 0xFFFF0000) >> 16) / 256.0;
		float green = float(data & 0xFFFF) / 256.0;
		hitColor = vec3(red, green, blue) / float(count);
		return true;
	}

	return false;
}

void TraceVoxelGrid(usampler3D voxels, vec3 rayPos, vec3 rayDir, out vec3 hitColor)
{
	vec3 voxelVolumeMax = vec3(VoxelSize * VoxelGridSize);
	vec3 voxelVolumeMin = -voxelVolumeMax;

	float tmin, tmax;
	aabbIntersectFast(rayPos, rayDir, 1.0 / rayDir, voxelVolumeMin, voxelVolumeMax, tmin, tmax);

	if (tmin > tmax || tmax < 0)
	{
		hitColor = vec3(0);
        return;
	}

	if (tmin > 0)
	{
		rayPos += rayDir * tmin;
	}

	vec3 voxelPos = rayPos.xyz * 0.5 / VoxelSize + VoxelGridSize / 2;
	ivec3 voxelIndex = ivec3(voxelPos);

	vec3 deltaDist = abs(vec3(1.0) / rayDir);
	vec3 raySign = sign(rayDir);
	ivec3 rayStep = ivec3(raySign);

	// Distance to next voxel in each axis
	vec3 sideDist = (raySign * (vec3(voxelIndex) - voxelPos) + (raySign * 0.5) + 0.5) * deltaDist;
	bvec3 mask;

	for (int i = 0; i < 1024; i++)
	{
		vec3 color;
		voxelIndex.z %= int(VoxelGridSize);
		if (UnpackVoxel(voxels, voxelIndex, color))
		{
			hitColor = vec3(mask) * vec3(1.0);
			hitColor = color;
			return;
		}

		// Find axis with maximum distance
		mask = lessThanEqual(sideDist.xyz, min(sideDist.yzx, sideDist.zxy));
		sideDist += vec3(mask) * deltaDist;
		voxelIndex += ivec3(mask) * rayStep;
	}

	hitColor = vec3(0);
}

void main()
{
	if (mode == 1) {
		outFragColor.rgb = texture(gBuffer0, inTexCoord).rgb;
	} else if (mode == 2) {
		outFragColor.rgb = texture(gBuffer1, inTexCoord).rgb * vec3(0.5, 0.5, 1.0) + vec3(0.5, 0.5, 0.0);
	} else if (mode == 3) {
		float depth = texture(depthStencil, inTexCoord).r;
		vec3 position = ScreenPosToViewPos(inTexCoord, depth, invProjMat);
		outFragColor.rgb = vec3(-position.z / 32.0);
	} else if (mode == 4) {
		outFragColor.rgb = vec3(texture(gBuffer0, inTexCoord).a);
	} else if (mode == 5) {
		vec4 rayPos = invViewMat * vec4(ScreenPosToViewPos(inTexCoord, 0, invProjMat), 1);
		vec4 rayDir = normalize(rayPos - (invViewMat * vec4(0, 0, 0, 1)));
		TraceVoxelGrid(voxelColor, rayPos.xyz, rayDir.xyz, outFragColor.rgb);
	}
}