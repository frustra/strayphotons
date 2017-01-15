#define USE_SHADOW_MAPPING

##import lib/lighting_util

float sampleOcclusion(sampler2D map, int i, vec3 shadowMapPos, vec3 shadowMapTexCoord) {
	float sampledDepth = texture(map, shadowMapTexCoord.xy * lightMapOffset[i].zw + lightMapOffset[i].xy).r;
	float fragmentDepth = (length(shadowMapPos) - lightClip[i].x) / (lightClip[i].y - lightClip[i].x);

	return step(0, -shadowMapPos.z) * smoothstep(fragmentDepth - 0.0001, fragmentDepth - 0.00005, sampledDepth);
}

float directOcclusion(sampler2D map, int i, vec3 shadowMapPos, vec3 shadowMapTexCoord, mat2 rotation0) {
	vec2 sampleRadius = 5.0 / textureSize(map, 0).xy;
	mat2 rotation1 = mat2(0.707, 0.707, -0.707, 0.707) * rotation0;
	float occlusion = 0;

	for (int s = 0; s < 8; s++) {
		vec2 offset = SpiralOffsets[s] * sampleRadius;

		occlusion += sampleOcclusion(map, i, shadowMapPos, shadowMapTexCoord + vec3(rotation0 * offset, 0));
		occlusion += sampleOcclusion(map, i, shadowMapPos, shadowMapTexCoord + vec3(rotation1 * offset, 0));
	}

	return min(occlusion / 16.0, 1);
}

vec3 evaluateBRDF(vec3 diffuseColor, vec3 specularColor, float roughness, vec3 L, vec3 V, vec3 N) {
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

#ifdef DIFFUSE_ONLY_SHADING
vec3 directShading(vec3 worldPosition, vec3 baseColor, vec3 normal, float roughness) {
#else
vec3 directShading(vec3 worldPosition, vec3 directionToView, vec3 baseColor, vec3 normal, float roughness, float metalness) {
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
		vec3 brdf = evaluateBRDF(baseColor, vec3(0.04), roughness, incidence, directionToView, normal);
#endif
		vec3 luminance = brdf * illuminance * currLightColor;

		// Spotlight attenuation.
		float cosSpotAngle = lightSpotAngleCos[i];
		float spotTerm = dot(incidence, -currLightDir);
		float spotFalloff = smoothstep(cosSpotAngle, 1, spotTerm) * step(-1, cosSpotAngle) + step(cosSpotAngle, -1);

		// Calculate direct occlusion.
		vec3 shadowMapPos = (lightView[i] * vec4(worldPosition, 1.0)).xyz; // Position of light view-space.
		vec3 shadowMapTexCoord = ViewPosToScreenPos(shadowMapPos, lightProj[i]);
		float occlusion = 1;

#ifdef USE_SHADOW_MAPPING
#ifdef USE_PCF
		occlusion = directOcclusion(shadowMap, i, shadowMapPos, shadowMapTexCoord, rotation);
#else
		occlusion = sampleOcclusion(shadowMap, i, shadowMapPos, shadowMapTexCoord);
#endif
#endif

		// Sum output.
		pixelLuminance += occlusion * luminance * spotFalloff;
	}

	return pixelLuminance;
}
