#version 450

#extension GL_OVR_multiview2 : enable
#extension GL_EXT_nonuniform_qualifier : enable
layout(num_views = 2) in;
layout(early_fragment_tests) in;

#define USE_PCF
#define SHADOWS_ENABLED
#define LIGHTING_GELS

#include "../lib/types_common.glsl"
#include "../lib/util.glsl"

layout(binding = 0) uniform sampler2D shadowMap;
layout(binding = 1) uniform sampler3D voxelRadiance;
layout(binding = 2) uniform sampler3D voxelNormals;

layout(set = 2, binding = 0) uniform sampler2D textures[];

layout(binding = 8) uniform VoxelStateUniform {
    VoxelState voxelInfo;
};

INCLUDE_LAYOUT(binding = 9)
#include "lib/exposure_state.glsl"

INCLUDE_LAYOUT(binding = 10)
#include "lib/view_states_uniform.glsl"

INCLUDE_LAYOUT(binding = 11)
#include "lib/light_data_uniform.glsl"

layout(location = 0) in vec3 inViewPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) flat in int baseColorTexID;
layout(location = 4) flat in int metallicRoughnessTexID;
layout(location = 5) flat in float emissiveScale;
layout(location = 0) out vec4 outFragColor;
layout(location = 1) out vec4 outTransparencyMask;

#include "../lib/shading.glsl"
#include "../lib/voxel_trace_shared.glsl"

void main() {
    ViewState view = views[gl_ViewID_OVR];

    vec4 baseColorAlpha = texture(textures[baseColorTexID], inTexCoord);
    vec3 baseColor = baseColorAlpha.rgb * baseColorAlpha.a;

    vec4 metallicRoughnessSample = texture(textures[metallicRoughnessTexID], inTexCoord);
    float roughness = metallicRoughnessSample.g;
    float metalness = metallicRoughnessSample.b;

    vec3 emissive = emissiveScale * baseColor;

    // Determine coordinates of fragment.
    vec2 screenPos = vec2(inTexCoord.x, 1 - inTexCoord.y);
    vec3 fragPosition = ScreenPosToViewPos(screenPos, 0, view.invProjMat);
    vec3 worldPosition = (view.invViewMat * vec4(inViewPos, 1.0)).xyz;
    vec3 worldFragPosition = (view.invViewMat * vec4(fragPosition, 1.0)).xyz;

    vec3 worldNormal = normalize(mat3(view.invViewMat) * inNormal);

    // Trace.
    vec3 rayDir = normalize(worldPosition - worldFragPosition);
    worldNormal *= sign(dot(worldNormal, rayDir));
    vec3 rayReflectDir = reflect(rayDir, worldNormal);

    vec3 indirectSpecular = vec3(0);
    {
        // specular
        vec3 directSpecularColor = mix(vec3(0.04), baseColor, metalness);
        if (any(greaterThan(directSpecularColor, vec3(0.0))) && roughness < 1.0) {
            float specularConeRatio = roughness * 0.8;
            vec4 sampleColor =
                ConeTraceGrid(specularConeRatio, worldPosition, rayReflectDir, worldNormal, gl_FragCoord.xy);

            vec3 brdf = EvaluateBRDFSpecularImportanceSampledGGX(directSpecularColor,
                roughness,
                rayReflectDir,
                -rayDir,
                worldNormal);
            indirectSpecular = sampleColor.rgb * brdf;
        }
    }

    vec3 indirectDiffuse = HemisphereIndirectDiffuse(worldPosition, worldNormal, gl_FragCoord.xy);

    vec3 directDiffuseColor = baseColor - baseColor * metalness;
    vec3 directLight = DirectShading(worldPosition, -rayDir, baseColor, worldNormal, worldNormal, roughness, metalness);

    vec3 indirectLight = indirectDiffuse * directDiffuseColor + indirectSpecular;
    vec3 totalLight = emissive + (directLight + indirectLight) * 0.04;

    outFragColor = vec4(totalLight * exposure, 1);
    outTransparencyMask = vec4(baseColorAlpha.rgb * baseColorAlpha.a, 1);
}
