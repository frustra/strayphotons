#version 430

#extension GL_OVR_multiview2 : enable
layout(num_views = 1) in;
layout(early_fragment_tests) in; // Force depth/stencil testing before shader invocation.

#include "../lib/util.glsl"

layout(location = 0) in vec3 inViewPos;
layout(location = 0) out vec4 outLinearDepth;

#include "../lib/types_common.glsl"

layout(binding = 10) uniform ViewState {
    mat4 view[2];
    mat4 projection[2];
	vec4 clip;
} viewState;

void main() {
	vec2 clip = gl_ViewID_OVR > 0 ? viewState.clip.zw : viewState.clip.xy;
	outLinearDepth.r = LinearDepth(inViewPos, clip);
}
