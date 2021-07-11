##import lib/lighting_util
##import lib/spatial_util

#ifdef INCLUDE_MIRRORS
#define MIRROR_SAMPLE
##import lib/shadow_sample
#undef MIRROR_SAMPLE
#endif
##import lib/shadow_sample

vec3 EvaluateBRDF(vec3 diffuseColor, vec3 specularColor, float roughness, vec3 L, vec3 V, vec3 N) {
	vec3 H = normalize(V + L);
	float NdotV = max(dot(N, V), 1e-5); // see [Lagarde/Rousiers 2014]
	float NdotL = max(dot(N, L), 1e-5);
	float NdotH = clamp(dot(N, H), 0.0, 0.99999);
	float VdotH = clamp(dot(V, H), 0.0, 0.99999);

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
	float NdotV = max(dot(N, V), 1e-5); // see [Lagarde/Rousiers 2014]
	float NdotL = max(dot(N, L), 1e-5);
	float NdotH = clamp(dot(N, H), 0.0, 0.99999);
	float VdotH = clamp(dot(V, H), 0.0, 0.99999);

	float D = BRDF_D_GGX(roughness, NdotH);
	float Vis = BRDF_V_Schlick(roughness, NdotV, NdotL);
	vec3 F = BRDF_F_Schlick(specularColor, VdotH);

	return D * Vis * F;
}

#ifdef DIFFUSE_ONLY_SHADING
vec3 DirectShading(vec3 worldPosition, vec3 baseColor, vec3 normal, vec3 flatNormal) {
#else
vec3 DirectShading(vec3 worldPosition, vec3 directionToView, vec3 baseColor, vec3 normal, vec3 flatNormal, float roughness, float metalness) {
#endif
	vec3 pixelLuminance = vec3(0);

#ifdef USE_PCF
	// Rotate PCF kernel by a random angle.
	float angle = InterleavedGradientNoise(gl_FragCoord.xy) * M_PI * 2.0;
	float s = sin(angle), c = cos(angle);
	mat2 rotation = mat2(c, s, -s, c);
#endif

	for (int i = 0; i < lightCount; i++) {
		vec3 sampleToLightRay = lights[i].position - worldPosition;
		vec3 incidence = normalize(sampleToLightRay);

		vec3 currLightDir = normalize(lights[i].direction);
		vec3 currLightColor = lights[i].tint;
		float illuminance = lights[i].illuminance;

		float notHasIllum = step(illuminance, 0);
		float hasIllum = 1.0 - notHasIllum;

		{
			float lightDistance = length(abs(lights[i].position - worldPosition));
			float lightDistanceSq = lightDistance * lightDistance;
			float falloff = 1.0 / (max(lightDistanceSq, punctualLightSizeSq));

			illuminance = notHasIllum * lights[i].intensity * falloff + hasIllum * illuminance;
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
		float spotFalloff = smoothstep(cosSpotAngle, 1, spotTerm) * notHasIllum + hasIllum;

		// Calculate direct occlusion.
		vec3 shadowMapPos = (lights[i].view * vec4(worldPosition, 1.0)).xyz; // Position of light view-space.
		vec3 surfaceNormal = normalize(mat3(lights[i].view) * flatNormal);
		float occlusion = step(lights[i].clip.x, -shadowMapPos.z);

		vec2 viewBounds = vec2(lights[i].invProj[0][0], lights[i].invProj[1][1]) * lights[i].clip.x;
		ShadowInfo info = ShadowInfo(
			i,
			shadowMapPos,
			lights[i].proj,
			lights[i].invProj,
			lights[i].mapOffset,
			lights[i].clip,
			vec4(-viewBounds, viewBounds * 2.0)
		);

#ifdef SHADOWS_ENABLED
#ifdef USE_PCF
		occlusion *= DirectOcclusion(info, surfaceNormal, rotation);
#else
		occlusion *= SimpleOcclusion(info);
#endif
#endif

		vec3 lightTint = vec3(spotFalloff);
#ifdef LIGHTING_GEL
		if (lights[i].gelId > 0) {
			vec2 coord = ViewPosToScreenPos(shadowMapPos, lights[i].proj).xy;
			lightTint = texture(lightingGel, coord).rgb * float(coord == clamp(coord, 0, 1));
		}
#endif

		// Sum output.
		pixelLuminance += occlusion * lightTint * luminance;
	}

#ifdef INCLUDE_MIRRORS
	for (int i = 0; i < mirrorData.count[0]; i++) {
		vec3 sourcePos = vec3(mirrorData.invViewMat[i] * vec4(0, 0, 0, 1));
		uint lightId = mirrorData.sourceLight[i];

		vec3 sampleToLightRay = sourcePos - worldPosition;
		vec3 incidence = normalize(sampleToLightRay);

		vec3 currLightColor = lights[lightId].tint;
		float illuminance = lights[lightId].illuminance;

		float notHasIllum = step(illuminance, 0);
		float hasIllum = 1.0 - notHasIllum;

		{
			float lightDistance = length(abs(sourcePos - worldPosition));
			float lightDistanceSq = lightDistance * lightDistance;
			float falloff = 1.0 / (max(lightDistanceSq, punctualLightSizeSq));

			illuminance = notHasIllum * lights[lightId].intensity * falloff + hasIllum * illuminance;
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
		vec3 currLightDir = normalize(mat3(mirrorData.invLightViewMat[i]) * vec3(0, 0, -1));
		float spotTerm = dot(incidence, -currLightDir);
		float spotFalloff = smoothstep(cosSpotAngle, 1, spotTerm) * notHasIllum + hasIllum;

		// Calculate direct occlusion.
		vec3 shadowMapPos = (mirrorData.viewMat[i] * vec4(worldPosition, 1.0)).xyz; // Position of light view-space.
		vec3 surfaceNormal = normalize(mat3(mirrorData.viewMat[i]) * flatNormal);
		float occlusion = step(mirrorData.clip[i].x, -shadowMapPos.z);

		ShadowInfo info = ShadowInfo(
			i,
			shadowMapPos,
			mirrorData.projMat[i],
			mirrorData.invProjMat[i],
			vec4(0, 0, 1, 1),
			mirrorData.clip[i],
			mirrorData.nearInfo[i]
		);

#ifdef SHADOWS_ENABLED
#ifdef USE_PCF
		occlusion *= DirectOcclusionMirror(info, surfaceNormal, rotation);
#else
		occlusion *= SimpleOcclusionMirror(info);
#endif
#endif

		vec3 lightTint = vec3(spotFalloff);
#ifdef LIGHTING_GEL
		if (lights[lightId].gelId > 0) {
			vec2 coord = ViewPosToScreenPos((mirrorData.lightViewMat[i] * vec4(worldPosition, 1.0)).xyz, lights[lightId].proj).xy;
			lightTint = texture(lightingGel, coord).rgb;
		}
#endif

		// Sum output.
		pixelLuminance += occlusion * lightTint * luminance;
	}
#endif

	return pixelLuminance;
}
