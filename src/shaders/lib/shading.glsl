#define USE_SHADOW_MAPPING

##import lib/lighting_util
##import lib/spatial_util

const float shadowBiasDistance = 0.04;

float SimpleOcclusion(int i, vec3 shadowMapPos) {
	vec3 texCoord = ViewPosToScreenPos(shadowMapPos, lightProj[i]);

	float shadowBias = shadowBiasDistance / (lightClip[i].y - lightClip[i].x);

	float testDepth = LinearDepth(shadowMapPos, lightClip[i]);
	float sampledDepth = texture(shadowMap, texCoord.xy * lightMapOffset[i].zw + lightMapOffset[i].xy).r;

	return step(0, -shadowMapPos.z) * smoothstep(testDepth - shadowBias, testDepth - shadowBias * 0.2, sampledDepth);
}

float SampleOcclusion(int i, vec3 shadowMapCoord, vec3 shadowMapPos, vec3 surfaceNormal, vec2 offset) {
	vec2 mapSize = textureSize(shadowMap, 0).xy * lightMapOffset[i].zw;
	vec2 texelSize = 1.0 / mapSize;
	vec3 texCoord = shadowMapCoord + vec3(offset * texelSize.xy, 0);

	vec2 halfTanFovXY = vec2(lightInvProj[i][0][0], lightInvProj[i][1][1]);
	vec3 rayDir = normalize(vec3(halfTanFovXY * (texCoord.xy * 2.0 - 1.0), -1.0));

	float t = dot(surfaceNormal, shadowMapPos) / dot(surfaceNormal, rayDir);
	vec3 hitPos = rayDir * t;

	float shadowBias = shadowBiasDistance / (lightClip[i].y - lightClip[i].x);

	float testDepth = LinearDepth(hitPos, lightClip[i]);
	testDepth = max(testDepth, LinearDepth(shadowMapPos, lightClip[i]) - shadowBias * 2.0);

	vec2 coord = texCoord.xy * lightMapOffset[i].zw + lightMapOffset[i].xy;
	vec3 sampledDepth = texture(shadowMap, coord).rrr;
	vec4 values = textureGather(shadowMap, coord, 0);
	sampledDepth.y = min(values.x, min(values.y, min(values.z, values.w)));
	sampledDepth.z = max(values.x, max(values.y, max(values.z, values.w)));

	float minTest = min(sampledDepth.y, testDepth - shadowBias);
	minTest = max(minTest, step(sampledDepth.z, testDepth - shadowBias * 2.0) * testDepth - shadowBias);

	return step(0, -shadowMapPos.z) * smoothstep(minTest, testDepth, sampledDepth.x);
}

// const float[5] gaussKernel = float[](0.06136, 0.24477, 0.38774, 0.24477, 0.06136);
const float[5][5] diskKernel = float[][](
    float[]( 0.0   , 0.5/17, 1.0/17, 0.5/17, 0.0    ),
    float[]( 0.5/17, 1.0/17, 1.0/17, 1.0/17, 0.5/17 ),
    float[]( 1.0/17, 1.0/17, 1.0/17, 1.0/17, 1.0/17 ),
    float[]( 0.5/17, 1.0/17, 1.0/17, 1.0/17, 0.5/17 ),
    float[]( 0.0   , 0.5/17, 1.0/17, 0.5/17, 0.0    )
);

const int kernelRadius = 2;

float DirectOcclusion(int i, vec3 shadowMapPos, vec3 surfaceNormal, mat2 rotation0) {
	vec3 shadowMapCoord = ViewPosToScreenPos(shadowMapPos, lightProj[i]);

	// return SampleOcclusion(i, shadowMapCoord, shadowMapPos, surfaceNormal, vec2(0));

	mat2 rotation1 = mat2(0.707, 0.707, -0.707, 0.707);// * rotation0;
	float occlusion = 0;

	// for (int s = 0; s < 8; s++) {
	// 	vec2 offset = SpiralOffsets[s] * 2;

	// 	occlusion += SampleOcclusion(i, shadowMapPos, surfaceNormal, rotation0 * offset);
	// 	occlusion += SampleOcclusion(i, shadowMapPos, surfaceNormal, rotation1 * offset);
	// }
	for (int x = -kernelRadius; x <= kernelRadius; x++) {
		for (int y = -kernelRadius; y <= kernelRadius; y++) {
			vec2 offset = vec2(x, y);

			occlusion += SampleOcclusion(i, shadowMapCoord, shadowMapPos, surfaceNormal, rotation0 * offset)
						 * diskKernel[x + kernelRadius][y + kernelRadius];
						// * gaussKernel[x + kernelRadius] * gaussKernel[y + kernelRadius];
		}
	}

	return occlusion;
}

vec3 EvaluateBRDF(vec3 diffuseColor, vec3 specularColor, float roughness, vec3 L, vec3 V, vec3 N) {
	vec3 H = normalize(V + L);
	float NdotV = abs(dot(N, V)) + 1e-5; // see [Lagarde/Rousiers 2014]
	float NdotL = abs(dot(N, L)) + 1e-5;
	float NdotH = saturate(dot(N, H));
	float VdotH = saturate(dot(V, H));

	// float D = BRDF_D_Blinn(roughness, NdotH);
	// float Vis = 0.25; // implicit case
	// vec3 F = specularColor; // no fresnel

	float D = BRDF_D_GGX(roughness, NdotH);
	float Vis = BRDF_V_Schlick(roughness, NdotV, NdotL);
	vec3 F = BRDF_F_Schlick(specularColor, VdotH);

	vec3 diffuse = BRDF_Diffuse_Lambert(diffuseColor);
	vec3 specular = D * Vis * F;

	return specular + diffuse;
}

vec3 EvaluateBRDFSpecularImportanceSampledGGX(vec3 specularColor, float roughness, vec3 L, vec3 V, vec3 N) {
	vec3 H = normalize(V + L);
	float NdotV = abs(dot(N, V)) + 1e-5; // see [Lagarde/Rousiers 2014]
	float NdotL = abs(dot(N, L)) + 1e-5;
	float VdotH = saturate(dot(V, H));

	float Vis = BRDF_V_Schlick(roughness, NdotV, NdotL);
	vec3 F = BRDF_F_Schlick(specularColor, VdotH);

	return Vis * F * VdotH; // * 4 / (NdotH * M_PI); // NdotH is 1 in this case
}

#ifdef DIFFUSE_ONLY_SHADING
vec3 DirectShading(vec3 worldPosition, vec3 baseColor, vec3 normal, float roughness) {
#else
vec3 DirectShading(vec3 worldPosition, vec3 directionToView, vec3 baseColor, vec3 normal, float roughness, float metalness) {
#endif
	vec3 pixelLuminance = vec3(0);

#ifdef USE_PCF
	// Rotate PCF kernel by a random angle.
	float angle = InterleavedGradientNoise(inTexCoord.xy * targetSize) * M_PI;
	float s = sin(angle), c = cos(angle);
	mat2 rotation = mat2(c, s, -s, c);
#endif

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
			illuminance = max(dot(normal, incidence), 0) * lightIntensity[i] * falloff;
		} else {
			// Given value is the orthogonal case, need to project to l.
			illuminance *= max(dot(normal, incidence), 0);
		}

		// Evaluate BRDF and calculate luminance.
#ifdef DIFFUSE_ONLY_SHADING
		vec3 brdf = BRDF_Diffuse_Lambert(baseColor);
#else
		vec3 diffuseColor = baseColor - baseColor * metalness;
		vec3 specularColor = mix(vec3(0.04), baseColor, metalness);
		vec3 brdf = EvaluateBRDF(diffuseColor, specularColor, roughness, incidence, directionToView, normal);
#endif
		vec3 luminance = brdf * illuminance * currLightColor;

		// Spotlight attenuation.
		float cosSpotAngle = lightSpotAngleCos[i];
		float spotTerm = dot(incidence, -currLightDir);
		float spotFalloff = smoothstep(cosSpotAngle, 1, spotTerm) * step(-1, cosSpotAngle) + step(cosSpotAngle, -1);

		// Calculate direct occlusion.
		vec3 shadowMapPos = (lightView[i] * vec4(worldPosition, 1.0)).xyz; // Position of light view-space.
		vec3 surfaceNormal = normalize(mat3(lightView[i]) * normal);
		float occlusion = 1;

#ifdef USE_SHADOW_MAPPING
#ifdef USE_PCF
		occlusion = DirectOcclusion(i, shadowMapPos, surfaceNormal, rotation);
#else
		occlusion = SimpleOcclusion(i, shadowMapPos);
#endif
#endif

		// Sum output.
		pixelLuminance += occlusion * spotFalloff * luminance;
	}

	return pixelLuminance;
}
