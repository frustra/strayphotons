#version 430

##import lib/util
##import voxel_shared
##import lib/lighting_util
##import lib/light_inputs
##import lib/shadow_sample

layout (binding = 0) uniform sampler2D baseColorTex;
// binding 1 = roughnessTex
// binding 2 = bumpTex
layout (binding = 3) uniform sampler2D shadowMap;

layout (binding = 0) uniform atomic_uint fragListSize;
layout (binding = 0, offset = 4) uniform atomic_uint nextComputeSize;

layout (binding = 0, r32ui) writeonly uniform uimage2D voxelFragList;
layout (binding = 1, r32ui) uniform uimage3D voxelData;

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inTexCoord;
layout (location = 2) in flat int inDirection;

in vec4 gl_FragCoord;

// Data format: [color.r, color.g], [color.b, radiance.r], [radiance.g, radiance.b], [count]

void main()
{
	vec4 diffuseColor = texture(baseColorTex, inTexCoord);
	if (diffuseColor.a < 0.5) discard;

	vec3 position = vec3(gl_FragCoord.xy / 2.0, gl_FragCoord.z * VoxelGridSize);
	position = AxisSwapReverse[abs(inDirection)-1] * (position - VoxelGridSize / 2);
	vec3 worldPosition = position * VoxelSize + VoxelGridCenter;
	position += VoxelGridSize / 2;

	vec3 pixelLuminance = vec3(0);

	for (int i = 0; i < lightCount; i++) {
		vec3 sampleToLightRay = lightPosition[i] - worldPosition;
		vec3 incidence = normalize(sampleToLightRay);
		vec3 currLightDir = normalize(lightDirection[i]);
		float falloff = 1;

		float illuminance = lightIlluminance[i];
		vec3 currLightColor = lightTint[i];

		if (illuminance == 0) {
			// Determine physically-based distance attenuation.
			float lightDistance = length(abs(lightPosition[i] - worldPosition));
			float lightDistanceSq = lightDistance * lightDistance;
			falloff = 1.0 / (max(lightDistanceSq, punctualLightSizeSq));

			// Calculate illuminance from intensity with E = L * n dot l.
			illuminance = max(dot(inNormal, incidence), 0) * lightIntensity[i] * falloff;
		} else {
			// Given value is the orthogonal case, need to project to l.
			illuminance *= max(dot(inNormal, incidence), 0);
		}

		// Evaluate BRDF and calculate luminance.
		vec3 diffuse = BRDF_Diffuse_Lambert(diffuseColor.rgb * 0.04);
		vec3 luminance = diffuse * illuminance * currLightColor;

		// Spotlight attenuation.
		float cosSpotAngle = lightSpotAngleCos[i];
		float spotTerm = dot(incidence, -currLightDir);
		float spotFalloff = smoothstep(cosSpotAngle, 1, spotTerm) * step(-1, cosSpotAngle) + step(cosSpotAngle, -1);

		// Calculate direct occlusion.
		vec3 shadowMapPos = (lightView[i] * vec4(worldPosition, 1.0)).xyz; // Position of light view-space.
		vec3 shadowMapTexCoord = ViewPosToScreenPos(shadowMapPos, lightProj[i]);
		float occlusion = 1;

		occlusion = sampleOcclusion(shadowMap, i, shadowMapPos, shadowMapTexCoord);

		// Sum output.
		pixelLuminance += occlusion * luminance * spotFalloff;
	}

	uint rg = (uint(diffuseColor.r * 0xFF) << 16) + uint(diffuseColor.g * 0xFF);
	uint br = (uint(diffuseColor.b * 0xFF) << 16) + uint(pixelLuminance.r * 0x7FF);
	uint gb = (uint(pixelLuminance.g * 0x7FF) << 16) + uint(pixelLuminance.b * 0x7FF);
	imageAtomicAdd(voxelData, ivec3(floor(position.x) * 4, position.yz), rg);
	imageAtomicAdd(voxelData, ivec3(floor(position.x) * 4 + 1, position.yz), br);
	imageAtomicAdd(voxelData, ivec3(floor(position.x) * 4 + 2, position.yz), gb);
	uint prevData = imageAtomicAdd(voxelData, ivec3(floor(position.x) * 4 + 3, position.yz), 1);

	if ((prevData & 0xFFFF) == 0) {
		uint index = atomicCounterIncrement(fragListSize);
		if (index % MipmapWorkGroupSize == 0) atomicCounterIncrement(nextComputeSize);

		uint packedData = (uint(position.x) & 0x3FF) << 20;
		packedData += (uint(position.y) & 0x3FF) << 10;
		packedData += uint(position.z) & 0x3FF;
		imageStore(voxelFragList, ivec2(index & MaxFragListMask[0], index >> FragListWidthBits[0]), uvec4(packedData));
	}
}
