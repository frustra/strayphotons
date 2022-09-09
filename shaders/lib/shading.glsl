#include "lighting_util.glsl"
#include "shadow_sample.glsl"
#include "spatial_util.glsl"

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
    float VdotH = clamp(dot(V, H), 0.0, 0.99999);

    float Vis = BRDF_V_Schlick(roughness, NdotV, NdotL);
    vec3 F = BRDF_F_Schlick(specularColor, VdotH);

    return Vis * F * VdotH;
}

#if defined(DIFFUSE_ONLY_SHADING)
vec3 DirectShading(vec3 worldPosition, vec3 baseColor, vec3 normal, vec3 flatNormal, float roughness, float metalness) {
#elif defined(TRANSPARENCY_SHADING)
vec3 DirectShading(vec3 worldPosition,
    vec3 directionToView,
    vec3 baseColor,
    vec3 normal,
    vec3 flatNormal,
    float roughness,
    float metalness,
    float scatterTerm) {
#else
vec3 DirectShading(vec3 worldPosition,
    vec3 directionToView,
    vec3 baseColor,
    vec3 normal,
    vec3 flatNormal,
    float roughness,
    float metalness) {
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
#ifdef TRANSPARENCY_SHADING
            illuminance *= abs(dot(normal, incidence));
#else
            illuminance *= max(dot(normal, incidence), 0);
#endif
        }

        // Evaluate BRDF and calculate luminance.
#ifdef DIFFUSE_ONLY_SHADING
        vec3 diffuseColor = baseColor * (1 - metalness * (1 - roughness));
        vec3 brdf = BRDF_Diffuse_Lambert(diffuseColor);
#else
    #ifdef TRANSPARENCY_SHADING
        vec3 diffuseColor = baseColor * (1 - metalness) * scatterTerm;
    #else
        vec3 diffuseColor = baseColor * (1 - metalness);
    #endif
        vec3 specularColor = mix(vec3(0.04), baseColor, metalness);
        vec3 brdf = EvaluateBRDF(diffuseColor, specularColor, max(roughness, 0.01), incidence, directionToView, normal);
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

        ShadowInfo info =
            ShadowInfo(shadowMapPos, lights[i].proj, lights[i].mapOffset, lights[i].clip, lights[i].bounds);

#ifdef SHADOWS_ENABLED
    #ifdef USE_VSM
        occlusion *= SampleVarianceShadowMap(info, 0.00002, 0.5);
    #else
        #ifdef USE_PCF
        occlusion *= DirectOcclusion(info, surfaceNormal, rotation);
        #else
        occlusion *= SimpleOcclusion(info);
        #endif
    #endif
#endif

        vec3 lightTint = vec3(spotFalloff);
#ifdef LIGHTING_GELS
        uint gelId = lights[i].gelId;
        if (gelId > 0) {
            vec2 coord = ViewPosToScreenPos(shadowMapPos, lights[i].proj).xy;

            vec4 cornerUVs[2] = lights[i].cornerUVs;
            coord = bilinearMix(cornerUVs[0].xy, cornerUVs[1].zw, cornerUVs[0].zw, cornerUVs[1].xy, coord);

            lightTint = texture(textures[gelId], vec2(coord.x, 1 - coord.y)).rgb * float(coord == clamp(coord, 0, 1));
        }
#endif

        // Sum output.
        pixelLuminance += occlusion * lightTint * luminance;
    }

    return pixelLuminance;
}
