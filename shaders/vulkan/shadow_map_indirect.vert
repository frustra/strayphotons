#version 460
#extension GL_EXT_shader_16bit_storage : require
#extension GL_OVR_multiview2 : enable
layout(num_views = 1) in;

#include "../lib/vertex_base.glsl"
#include "../lib/types_common.glsl"

layout(location = 0) in vec3 inPos;
layout(location = 0) out vec3 outViewPos;

#include "lib/draw_params.glsl"
layout(std430, set = 1, binding = 0) readonly buffer DrawParamsList {
	DrawParams drawParams[];
};

#include "lib/view_states_uniform.glsl"

void main() {
	mat4 modelMat = drawParams[gl_BaseInstance].modelMat;
	outViewPos = vec3(views[gl_ViewID_OVR].viewMat * modelMat * vec4(inPos, 1.0));
	gl_Position = views[gl_ViewID_OVR].projMat * vec4(outViewPos, 1.0);
}
