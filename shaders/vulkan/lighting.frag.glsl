#extension GL_OVR_multiview2 : enable
#extension GL_EXT_nonuniform_qualifier : enable
layout(num_views = 2) in;

#define SHADOWS_ENABLED
#define LIGHTING_GELS

#include "../lib/types_common.glsl"
#include "../lib/util.glsl"
#include "../lib/voxel_shared.glsl"

layout(binding = 0) uniform sampler2DArray gBuffer0;
layout(binding = 1) uniform sampler2DArray gBuffer1;
layout(binding = 2) uniform sampler2DArray gBuffer2;
layout(binding = 3) uniform sampler2DArray gBufferDepth;
layout(binding = 4) uniform sampler2D shadowMap;
layout(binding = 5) uniform sampler3D voxelRadiance;
layout(binding = 6) uniform sampler3D voxelNormals;

layout(set = 1, binding = 0) uniform sampler2D textures[];

layout(set = 2, binding = 0) uniform sampler3D voxelLayersIn[6];

layout(binding = 8) uniform VoxelStateUniform {
    VoxelState voxelInfo;
};

INCLUDE_LAYOUT(binding = 9)
#include "lib/exposure_state.glsl"

INCLUDE_LAYOUT(binding = 10)
#include "lib/view_states_uniform.glsl"

INCLUDE_LAYOUT(binding = 11)
#include "lib/light_data_uniform.glsl"

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outFragColor;

#include "../lib/shading.glsl"
#include "../lib/voxel_trace_shared.glsl"

layout(constant_id = 0) const uint MODE = 1;

void main() {
    ViewState view = views[gl_ViewID_OVR];

    vec4 gb0 = texture(gBuffer0, vec3(inTexCoord, gl_ViewID_OVR));
    vec4 gb1 = texture(gBuffer1, vec3(inTexCoord, gl_ViewID_OVR));
    vec4 gb2 = texture(gBuffer2, vec3(inTexCoord, gl_ViewID_OVR));
    float depth = texture(gBufferDepth, vec3(inTexCoord, gl_ViewID_OVR)).r;

    vec3 baseColor = gb0.rgb;
    float roughness = gb2.r;
    float metalness = gb0.a;
    float emissiveScale = gb1.b;
    vec3 viewNormal = DecodeNormal(gb1.rg);
    vec3 flatViewNormal = viewNormal; // DecodeNormal(gb1.ba);

    vec3 emissive = emissiveScale * baseColor;

    // Determine coordinates of fragment.
    vec2 screenPos = vec2(inTexCoord.x, 1 - inTexCoord.y);
    vec3 fragPosition = ScreenPosToViewPos(screenPos, 0, view.invProjMat);
    vec3 viewPosition = ScreenPosToViewPos(screenPos, depth, view.invProjMat);
    vec3 worldPosition = (view.invViewMat * vec4(viewPosition, 1.0)).xyz;
    vec3 worldFragPosition = (view.invViewMat * vec4(fragPosition, 1.0)).xyz;

    vec3 worldNormal = mat3(view.invViewMat) * viewNormal;
    vec3 flatWorldNormal = mat3(view.invViewMat) * flatViewNormal;

    // Trace.
    vec3 rayDir = normalize(worldPosition - worldFragPosition);
    vec3 rayReflectDir = reflect(rayDir, worldNormal);

    vec3 indirectSpecular = vec3(0);
    {
        // specular
        vec3 directSpecularColor = mix(vec3(0.04), baseColor, metalness);
        if (any(greaterThan(directSpecularColor, vec3(0.0))) && roughness < 1.0) {
            float specularConeRatio = roughness * 0.8;
            vec4 sampleColor =
                ConeTraceGrid(specularConeRatio, worldPosition, rayReflectDir, flatWorldNormal, gl_FragCoord.xy);

            vec3 brdf = EvaluateBRDFSpecularImportanceSampledGGX(directSpecularColor,
                roughness,
                rayReflectDir,
                -rayDir,
                worldNormal);
            indirectSpecular = sampleColor.rgb * brdf;
        }
    }

    // vec3 indirectDiffuse = HemisphereIndirectDiffuse(worldPosition, worldNormal, gl_FragCoord.xy);
    vec3 voxelPos = (voxelInfo.worldToVoxel * vec4(worldPosition, 1.0)).xyz;
    vec3 voxelNormal = normalize(mat3(voxelInfo.worldToVoxel) * worldNormal);

    // int axis = DominantAxis(worldNormal);
    // if (axis < 0) {
    //     axis = -axis + 2;
    // } else {
    //     axis -= 1;
    // }
    vec4 value = vec4(0);
    for (int i = 0; i < 6; i++) {
        // vec4 sampleValue = texelFetch(voxelLayersIn[i], ivec3(voxelPos), 0);
        vec4 sampleValue = texture(voxelLayersIn[i], voxelPos / voxelInfo.gridSize);
        value += sampleValue * max(0, dot(AxisDirections[i], voxelNormal));
    }
    // vec4 value = texelFetch(voxelLayersIn[axis], ivec3(voxelPos + AxisDirections[axis]), 0);
    // vec4 value = texture(voxelLayersIn[axis], (voxelPos + AxisDirections[axis]) / voxelInfo.gridSize);

    vec3 indirectDiffuse = value.rgb;

    vec3 directDiffuseColor = baseColor * (1 - metalness);
    vec3 directLight =
        DirectShading(worldPosition, -rayDir, baseColor, worldNormal, flatWorldNormal, roughness, metalness);

    vec3 indirectLight = indirectDiffuse * directDiffuseColor + indirectSpecular;
    vec3 totalLight = emissive + directLight + indirectLight;

    if (MODE == 0) { // Direct only
        outFragColor = vec4(directLight, 1.0);
    } else if (MODE == 2) { // Indirect lighting
        outFragColor = vec4(indirectLight, 1.0);
    } else if (MODE == 3) { // diffuse
        outFragColor = vec4(indirectDiffuse, 1.0);
    } else if (MODE == 4) { // specular
        outFragColor = vec4(indirectSpecular, 1.0);
    } else { // Full lighting
        outFragColor = vec4(totalLight, 1.0);
    }

    outFragColor.rgb *= exposure;
}
