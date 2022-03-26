#version 460
#extension GL_EXT_shader_16bit_storage : require

#include "../lib/types_common.glsl"
#include "../lib/util.glsl"
#include "../lib/vertex_base.glsl"

layout(location = 0) in vec3 inPos;
layout(location = 0) out vec3 outViewPos;

#include "lib/draw_params.glsl"

INCLUDE_LAYOUT(binding = 0)
#include "lib/view_states_uniform.glsl"

layout(std430, set = 1, binding = 0) readonly buffer DrawParamsList {
    DrawParams drawParams[];
};

void main() {
    outViewPos = vec3(views[0].viewMat * vec4(inPos, 1.0));
    gl_Position = views[0].projMat * vec4(outViewPos, 1.0);
}
