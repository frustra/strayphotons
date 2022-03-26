#version 430

layout(early_fragment_tests) in; // Force depth/stencil testing before shader invocation.

#include "../lib/util.glsl"

layout(location = 0) in vec3 inViewPos;
layout(location = 0) out vec4 outLinearDepth;

#include "../lib/types_common.glsl"

INCLUDE_LAYOUT(binding = 0)
#include "lib/view_states_uniform.glsl"

void main() {
    outLinearDepth.x = LinearDepth(inViewPos, views[0].clip);
}
