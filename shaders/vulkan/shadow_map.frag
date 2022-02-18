#version 430

#extension GL_OVR_multiview2 : enable
layout(num_views = 1) in;
layout(early_fragment_tests) in; // Force depth/stencil testing before shader invocation.

#include "../lib/util.glsl"

layout(location = 0) in vec3 inViewPos;
layout(location = 0) out vec4 outLinearDepth;

#include "../lib/types_common.glsl"
#include "lib/view_states_uniform.glsl"

void main() {
    outLinearDepth.r = LinearDepth(inViewPos, views[gl_ViewID_OVR].clip);
}
