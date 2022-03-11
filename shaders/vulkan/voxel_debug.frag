#version 450

#extension GL_OVR_multiview2 : enable
layout(num_views = 2) in;

#include "../lib/types_common.glsl"
#include "../lib/util.glsl"

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outFragColor;

INCLUDE_LAYOUT(binding = 0)
#include "lib/view_states_uniform.glsl"

layout(binding = 1) uniform VoxelStateUniform {
    VoxelState voxelInfo;
};

INCLUDE_LAYOUT(binding = 2)
#include "lib/exposure_state.glsl"

layout(binding = 3) uniform sampler3D voxelRadiance;
layout(binding = 4) uniform sampler2DArray overlayTex;

float GetVoxelNearest(vec3 position, out vec3 radiance) {
    vec4 radianceData = texelFetch(voxelRadiance, ivec3(position), 0);
    radiance = radianceData.rgb;
    return radianceData.a;
}

float TraceVoxelGrid(vec3 rayWorldPos, vec3 rayWorldDir, out vec3 hitRadiance) {
    vec3 rayVoxelPos = (voxelInfo.worldToVoxel * vec4(rayWorldPos, 1.0)).xyz;
    vec3 rayVoxelDir = normalize(mat3(voxelInfo.worldToVoxel) * rayWorldDir);

    ivec3 voxelIndex = ivec3(rayVoxelPos);

    vec3 deltaDist = abs(vec3(1.0) / rayVoxelDir);
    vec3 raySign = sign(rayVoxelDir);
    ivec3 rayStep = ivec3(raySign);

    // Distance to next voxel in each axis
    vec3 sideDist = (raySign * (vec3(voxelIndex) - rayVoxelPos) + (raySign * 0.5) + 0.5) * deltaDist;
    int maxIterations = int(max(voxelInfo.gridSize.x, max(voxelInfo.gridSize.y, voxelInfo.gridSize.z)) * 3);
    bvec3 mask;

    for (int i = 0; i < maxIterations; i++) {
        vec3 radiance;
        float alpha = GetVoxelNearest(rayVoxelPos + rayVoxelDir * 0.001, radiance);
        if (alpha > 0) {
            hitRadiance = radiance;
            return alpha;
        }

        // Find axis with minimum distance
        mask = lessThanEqual(sideDist.xyz, min(sideDist.yzx, sideDist.zxy));
        rayVoxelPos += rayVoxelDir * dot(vec3(mask), sideDist);
        voxelIndex += ivec3(mask) * rayStep;
        sideDist = (raySign * (vec3(voxelIndex) - rayVoxelPos) + (raySign * 0.5) + 0.5) * deltaDist;
    }

    hitRadiance = vec3(0);
    return 0;
}

void main() {
    ViewState view = views[gl_ViewID_OVR];

    vec4 rayPos = view.invViewMat *
                  vec4(ScreenPosToViewPos(vec2(inTexCoord.x, 1 - inTexCoord.y), 0, view.invProjMat), 1);
    vec3 rayDir = normalize(rayPos.xyz - view.invViewMat[3].xyz);

    vec3 sampleRadiance;
    TraceVoxelGrid(rayPos.xyz, rayDir, sampleRadiance);

    vec3 overlay = texture(overlayTex, vec3(inTexCoord, gl_ViewID_OVR)).rgb; // pre-exposed
    outFragColor = vec4(mix(sampleRadiance * exposure, overlay, 0.125), 1.0);
}
