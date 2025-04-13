/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#version 460
#extension GL_OVR_multiview2 : enable
layout(num_views = 2) in;

#include "../lib/types_common.glsl"
#include "../lib/util.glsl"
#include "../lib/vertex_base.glsl"

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out vec3 outRadiance;
layout(location = 2) out float outScale;

INCLUDE_LAYOUT(binding = 0)
#include "lib/view_states_uniform.glsl"

INCLUDE_LAYOUT(binding = 1)
#include "lib/exposure_state.glsl"

layout(binding = 2) uniform sampler2DArray gBaseColor;
layout(binding = 3) uniform sampler2DArray gNormalEmissive;
layout(binding = 4) uniform sampler2DArray gRoughnessMetalic;
layout(binding = 5) uniform usampler2DArray gDecals;

layout(push_constant) uniform PushConstants {
    vec3 radiance;
    float radius;
    vec3 point;
};

vec2 uvs[4] = vec2[](vec2(0, 0), vec2(0, 1), vec2(1, 0), vec2(1, 1));
float offset1Dir[4] = float[](1, 1, -1, -1);
float offset2Dir[4] = float[](1, -1, 1, -1);

void main() {
    ViewState view = views[gl_ViewID_OVR];
    mat4 viewProjMat = view.projMat * view.viewMat;

    vec3 pointOrigin = point;

    // move toward view to avoid depth fail
    vec3 viewDir = pointOrigin - view.invViewMat[3].xyz;
    pointOrigin -= viewDir * 0.001;

    // determine screen texcoords of vertex
    vec4 pointProj = viewProjMat * vec4(pointOrigin, 1);
    vec2 pointScreen = pointProj.xy / pointProj.w;
    vec2 pointScreenUV = pointScreen * 0.5 + 0.5;
    pointScreenUV.y = 1 - pointScreenUV.y;

    // look up normal to define plane for generated quad
    vec3 viewNormal = DecodeNormal(texture(gNormalEmissive, vec3(pointScreenUV, gl_ViewID_OVR)).rg);
    vec3 worldNormal = mat3(view.invViewMat) * viewNormal;

    vec3 offset1 = OrthogonalVec3(worldNormal) * radius;
    vec3 offset2 = normalize(cross(worldNormal, offset1)) * radius;
    vec3 offset = offset1 * offset1Dir[gl_VertexIndex] + offset2 * offset2Dir[gl_VertexIndex];
    vec3 worldPos = pointOrigin + offset;

    // scale up quad size to a minimum radius of several pixels
    vec4 offsetProj = viewProjMat * vec4(worldPos, 1);
    vec2 offsetScreen = offsetProj.xy / offsetProj.w;

    outScale = 1;
    float ndcMinRadius = max(view.invExtents.x, view.invExtents.y) * 3;
    float ndcRadius = distance(offsetScreen, pointScreen);
    if (ndcRadius < ndcMinRadius) {
        // make the quad proportionally larger and scale down fragment blending weight
        outScale = ndcRadius / ndcMinRadius;
        offset /= outScale;
        worldPos = pointOrigin + offset;
        offsetProj = viewProjMat * vec4(worldPos, 1);
    }

    gl_Position = offsetProj;
    outTexCoord = uvs[gl_VertexIndex];

    // contact radiance is determined by the material at the contact point
    vec3 baseColor = texture(gBaseColor, vec3(pointScreenUV, gl_ViewID_OVR)).rgb;
    uvec4 decalIDs = texture(gDecals, vec3(pointScreenUV, gl_ViewID_OVR));
	// TODO: Apply decals to base color
    float metallic = texture(gRoughnessMetalic, vec3(pointScreenUV, gl_ViewID_OVR)).g;
    outRadiance = exposure * radiance * 100 * (baseColor + 0.05) * metallic;
}
