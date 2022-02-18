#version 460
#extension GL_OVR_multiview2 : enable
layout(num_views = 2) in;

#include "../lib/types_common.glsl"
#include "../lib/vertex_base.glsl"

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out vec4 outColor;

#include "lib/view_states_uniform.glsl"

vec2 positions[4] = vec2[](vec2(-0.5, 0.5), vec2(-0.5, -0.5), vec2(0.5, 0.5), vec2(0.5, -0.5));

vec2 uvs[4] = vec2[](vec2(0, 0), vec2(0, 1), vec2(1, 0), vec2(1, 1));

layout(push_constant) uniform PushConstants {
    mat4 transform;
    vec3 colorScale;
}
constants;

void main() {
    ViewState view = views[gl_ViewID_OVR];
    gl_Position = view.projMat * view.viewMat * constants.transform * vec4(positions[gl_VertexIndex], 0, 1);
    outTexCoord = uvs[gl_VertexIndex];
    outColor = vec4(constants.colorScale, 1);
}
