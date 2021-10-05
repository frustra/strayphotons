#version 430

#extension GL_OVR_multiview2 : enable
layout(num_views = 1) in;

#include "../lib/vertex_base.glsl"

layout(location = 0) in vec3 inPos;
layout(location = 0) out vec3 outViewPos;

layout(push_constant) uniform PushConstants {
    mat4 model;
} constants;

layout(binding = 10) uniform ViewState {
    mat4 view[2];
    mat4 projection[2];
    vec2 clip[2];
} viewState;

void main() {
	outViewPos = vec3(viewState.view[gl_ViewID_OVR] * constants.model * vec4(inPos, 1.0));
	gl_Position = viewState.projection[gl_ViewID_OVR] * vec4(outViewPos, 1.0);
}
