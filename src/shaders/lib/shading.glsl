#define USE_SHADOW_MAPPING

##import lib/lighting_util
##import lib/spatial_util

float SampleOcclusion(int i, vec3 shadowMapPos, vec3 shadowMapTexCoord) {
	float sampledDepth = texture(shadowMap, shadowMapTexCoord.xy * lightMapOffset[i].zw + lightMapOffset[i].xy).r;
	float fragmentDepth = WarpDepth(shadowMapPos, lightClip[i], 0).x;

	return step(0, -shadowMapPos.z) * smoothstep(fragmentDepth - 0.0005, fragmentDepth - 0.0001, sampledDepth);
}

float Chebyshev(vec2 moments, float depth) {
	float p = step(depth, moments.x);
	float variance = max(moments.y - moments.x*moments.x, -0.001);
	float d = depth - moments.x;
	// Low value adjusts light leak reduction
	float p_max = linstep(0.3, 1.0, variance / (variance + d*d));
	return saturate(max(p, p_max));
}

float VarianceOcclusion(int i, vec3 shadowMapPos, vec3 shadowMapTexCoord) {
	vec2 depth = WarpDepth(shadowMapPos, lightClip[i], 0.0001);
	vec2 coord = shadowMapTexCoord.xy * lightMapOffset[i].zw + lightMapOffset[i].xy;

	vec4 moments = texture(shadowMap, coord);
	return step(0, -shadowMapPos.z) * min(Chebyshev(moments.xz, depth.x), Chebyshev(moments.yw, depth.y));
}

float DirectOcclusion(int i, vec3 shadowMapPos, vec3 shadowMapTexCoord, mat2 rotation0) {
	// return VarianceOcclusion(i, shadowMapPos, shadowMapTexCoord);

	vec2 sampleRadius = 1.0 / textureSize(shadowMap, 0).xy;

	mat2 rotation1 = mat2(0.707, 0.707, -0.707, 0.707);// * rotation0;
	float occlusion = 0;

	// for (int s = 0; s < 8; s++) {
	// 	vec2 offset = SpiralOffsets[s] * sampleRadius;

	// 	occlusion += SampleOcclusion(i, shadowMapPos, shadowMapTexCoord + vec3(rotation0 * offset, 0));
	// 	// occlusion += SampleOcclusion(i, shadowMapPos, shadowMapTexCoord + vec3(rotation1 * offset, 0));
	// }
	for (int x = -1; x <= 1; x++) {
		for (int y = -1; y <= 1; y++) {
			vec2 offset = vec2(x, y) * sampleRadius;

			occlusion += VarianceOcclusion(i, shadowMapPos, shadowMapTexCoord + vec3(rotation1 * offset, 0));
		}
	}

	return occlusion / 9.0;
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
		vec3 shadowMapTexCoord = ViewPosToScreenPos(shadowMapPos, lightProj[i]);
		float occlusion = 1;

#ifdef USE_SHADOW_MAPPING
#ifdef USE_PCF
		occlusion = DirectOcclusion(i, shadowMapPos, shadowMapTexCoord, rotation);
#else
		occlusion = SampleOcclusion(i, shadowMapPos, shadowMapTexCoord);
#endif
#endif

		// Sum output.
		pixelLuminance += occlusion * luminance * spotFalloff;
	}

	return pixelLuminance;
}
