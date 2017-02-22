#define USE_SHADOW_MAPPING

##import lib/lighting_util
##import lib/spatial_util

const float shadowBiasDistance = 0.04;

#ifdef INCLUDE_MIRRORS
float MirrorOcclusion(int i, vec3 shadowMapPos) {
	vec3 texCoord = ViewPosToScreenPos(shadowMapPos, mirrorData.projMat[i]);

	if (texCoord.xy != clamp(texCoord.xy, 0, 1)) return 0;

	float shadowBias = shadowBiasDistance / (mirrorData.clip[i].y - mirrorData.clip[i].x);

	float testDepth = LinearDepth(shadowMapPos, mirrorData.clip[i]);
	float sampledDepth = texture(mirrorShadowMap, vec3(texCoord.xy, i)).r;

	return step(0, -shadowMapPos.z) * smoothstep(testDepth - shadowBias, testDepth - shadowBias * 0.2, sampledDepth);
}
#endif

float SimpleOcclusion(int i, vec3 shadowMapPos) {
	vec3 texCoord = ViewPosToScreenPos(shadowMapPos, lights[i].proj);

	float shadowBias = shadowBiasDistance / (lights[i].clip.y - lights[i].clip.x);

	float testDepth = LinearDepth(shadowMapPos, lights[i].clip);
	float sampledDepth = texture(shadowMap, texCoord.xy * lights[i].mapOffset.zw + lights[i].mapOffset.xy).r;

	return step(0, -shadowMapPos.z) * smoothstep(testDepth - shadowBias, testDepth - shadowBias * 0.2, sampledDepth);
}

float SampleOcclusion(int i, vec3 shadowMapCoord, vec3 shadowMapPos, vec3 surfaceNormal, vec2 offset) {
	vec2 mapSize = textureSize(shadowMap, 0).xy * lights[i].mapOffset.zw;
	vec2 texelSize = 1.0 / mapSize;
	vec3 texCoord = shadowMapCoord + vec3(offset * texelSize.xy, 0);

	vec2 halfTanFovXY = vec2(lights[i].invProj[0][0], lights[i].invProj[1][1]);
	vec3 rayDir = normalize(vec3(halfTanFovXY * (texCoord.xy * 2.0 - 1.0), -1.0));

	float t = dot(surfaceNormal, shadowMapPos) / dot(surfaceNormal, rayDir);
	vec3 hitPos = rayDir * t;

	float shadowBias = shadowBiasDistance / (lights[i].clip.y - lights[i].clip.x);

	float testDepth = LinearDepth(hitPos, lights[i].clip);
	testDepth = max(testDepth, LinearDepth(shadowMapPos, lights[i].clip) - shadowBias * 2.0);

	vec2 coord = texCoord.xy * lights[i].mapOffset.zw + lights[i].mapOffset.xy;
	vec3 sampledDepth = vec3(texture(shadowMap, coord).r, 0, 0);
	vec4 values = textureGather(shadowMap, coord, 0);
	sampledDepth.y = min(values.x, min(values.y, min(values.z, values.w)));
	sampledDepth.z = max(values.x, max(values.y, max(values.z, values.w)));

	float minTest = min(sampledDepth.y, testDepth - shadowBias);
	minTest = max(minTest, step(sampledDepth.z, testDepth - shadowBias * 2.0) * testDepth - shadowBias);

	return step(0, -shadowMapPos.z) * smoothstep(0.0, 0.2, -dot(surfaceNormal, rayDir)) * smoothstep(minTest, testDepth, sampledDepth.x);
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
	vec3 shadowMapCoord = ViewPosToScreenPos(shadowMapPos, lights[i].proj);

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

	return smoothstep(0.3, 1.0, occlusion);
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
vec3 DirectShading(vec3 worldPosition, vec3 baseColor, vec3 normal, vec3 flatNormal, float roughness) {
#else
vec3 DirectShading(vec3 worldPosition, vec3 directionToView, vec3 baseColor, vec3 normal, vec3 flatNormal, float roughness, float metalness) {
#endif
	vec3 pixelLuminance = vec3(0);

#ifdef USE_PCF
	// Rotate PCF kernel by a random angle.
	float angle = InterleavedGradientNoise(inTexCoord.xy * targetSize) * M_PI;
	float s = sin(angle), c = cos(angle);
	mat2 rotation = mat2(c, s, -s, c);
#endif

	for (int i = 0; i < lightCount; i++) {
		vec3 sampleToLightRay = lights[i].position - worldPosition;
		vec3 incidence = normalize(sampleToLightRay);
		vec3 currLightDir = normalize(lights[i].direction);
		float falloff = 1;

		float illuminance = lights[i].illuminance;
		vec3 currLightColor = lights[i].tint;

		if (illuminance == 0) {
			// Determine physically-based distance attenuation.
			float lightDistance = length(abs(lights[i].position - worldPosition));
			float lightDistanceSq = lightDistance * lightDistance;
			falloff = 1.0 / (max(lightDistanceSq, punctualLightSizeSq));

			// Calculate illuminance from intensity with E = L * n dot l.
			illuminance = max(dot(normal, incidence), 0) * lights[i].intensity * falloff;
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
		float cosSpotAngle = lights[i].spotAngleCos;
		float spotTerm = dot(incidence, -currLightDir);
		float spotFalloff = smoothstep(cosSpotAngle, 1, spotTerm) * step(-1, cosSpotAngle) + step(cosSpotAngle, -1);

		// Calculate direct occlusion.
		vec3 shadowMapPos = (lights[i].view * vec4(worldPosition, 1.0)).xyz; // Position of light view-space.
		vec3 surfaceNormal = normalize(mat3(lights[i].view) * flatNormal);
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

#ifdef INCLUDE_MIRRORS
	for (int i = 0; i < mirrorData.count; i++) {
		vec3 sourcePos = vec3(mirrorData.invViewMat[i] * vec4(0, 0, 0, 1));
		uint lightId = (mirrorData.list[i] >> 16) & 0xFFFF;

		vec3 sampleToLightRay = sourcePos - worldPosition;
		vec3 incidence = normalize(sampleToLightRay);
		float falloff = 1;

		float illuminance = lights[lightId].illuminance;
		vec3 currLightColor = lights[lightId].tint;

		if (illuminance == 0) {
			// Determine physically-based distance attenuation.
			float lightDistance = length(abs(sourcePos - worldPosition));
			float lightDistanceSq = lightDistance * lightDistance;
			falloff = 1.0 / (max(lightDistanceSq, punctualLightSizeSq));

			// Calculate illuminance from intensity with E = L * n dot l.
			illuminance = max(dot(normal, incidence), 0) * lights[lightId].intensity * falloff;
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
		float cosSpotAngle = lights[lightId].spotAngleCos;
		vec3 mirrorNormal = mat3(mirrorData.invViewMat[i]) * vec3(0, 0, -1);
		vec3 currLightDir = reflect(lights[lightId].direction, mirrorNormal);
		float spotTerm = dot(incidence, -currLightDir);
		float spotFalloff = smoothstep(cosSpotAngle, 1, spotTerm) * step(-1, cosSpotAngle) + step(cosSpotAngle, -1);

		// Calculate direct occlusion.
		vec3 shadowMapPos = (mirrorData.viewMat[i] * vec4(worldPosition, 1.0)).xyz; // Position of light view-space.
		vec3 surfaceNormal = normalize(mat3(mirrorData.viewMat[i]) * flatNormal);
		float occlusion = 1;

		occlusion = MirrorOcclusion(i, shadowMapPos);

		// Sum output.
		pixelLuminance += occlusion * spotFalloff * luminance;
	}
#endif

	return pixelLuminance;
}
