#version 430

layout(early_fragment_tests) in; // Force depth/stencil testing before shader invocation.

#include "../lib/util.glsl"

layout(location = 0) in flat uint inOpticID;

#include "../lib/types_common.glsl"

layout(binding = 1) writeonly buffer OpticVisibility {
    uint visibility[MAX_LIGHTS][MAX_OPTICS];
};

layout(push_constant) uniform PushConstants {
    uint lightIndex;
};

void main() {
    if (inOpticID > 0) visibility[lightIndex][inOpticID - 1] = 1;
}
