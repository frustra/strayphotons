#version 460
#extension GL_OVR_multiview2 : enable
layout(num_views = 2) in;

#include "../lib/types_common.glsl"
#include "../lib/vertex_base.glsl"

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out vec3 outWorldPos;
layout(location = 2) out float outScale;

#include "lib/view_states_uniform.glsl"

layout(push_constant) uniform PushConstants {
    vec3 color;
    float radius;
    vec3 start;
    vec3 end;
    float time;
}
constants;

vec2 uvs[4] = vec2[](vec2(0, 0), vec2(0, 1), vec2(1, 0), vec2(1, 1));
float offsetDir[4] = float[](1, 1, -1, -1);

void main() {
    ViewState view = views[gl_ViewID_OVR];
    mat4 viewProjMat = view.projMat * view.viewMat;

    vec3 lineVertex = (gl_VertexIndex == 1 || gl_VertexIndex == 3) ? constants.end : constants.start;
    vec3 viewDir = lineVertex - view.invViewMat[3].xyz;
    vec3 lineDir = constants.end - constants.start;

    vec3 offset = normalize(cross(lineDir, viewDir)) * constants.radius * offsetDir[gl_VertexIndex];
    vec3 worldPos = lineVertex + offset;

    // scale up quad size to a minimum radius of ~1 pixels
    vec4 lineVertexProj = viewProjMat * vec4(lineVertex, 1);
    vec2 lineVertexScreen = lineVertexProj.xy / lineVertexProj.w;
    vec4 offsetProj = viewProjMat * vec4(worldPos, 1);
    vec2 offsetScreen = offsetProj.xy / offsetProj.w;

    float ndcMinRadius = max(view.invExtents.x, view.invExtents.y) * 2;
    float ndcRadius = distance(offsetScreen, lineVertexScreen);
    if (ndcRadius < ndcMinRadius) {
        // make the quad proportionally larger and scale down fragment blending weight
        outScale = ndcRadius / ndcMinRadius;
        offset /= outScale;
        worldPos = lineVertex + offset;
        offsetProj = viewProjMat * vec4(worldPos, 1);
    } else {
        outScale = 1;
    }

    gl_Position = offsetProj;
    outTexCoord = uvs[gl_VertexIndex];
    outWorldPos = worldPos;
}
