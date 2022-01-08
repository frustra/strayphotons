#version 460

#extension GL_OVR_multiview2 : enable
layout(num_views = 1) in;

#include "../lib/vertex_base.glsl"
#include "../lib/types_common.glsl"

layout(location = 0) in vec3 inPos;
layout(location = 0) out vec3 outViewPos;

layout(std140, set = 1, binding = 0) readonly buffer Scene {
	mat4 modelMatrixes[];
} scene;

// layout(push_constant) uniform PushConstants {
// 	mat4 model;
// } constants;

#include "lib/view_states_uniform.glsl"

void main() {
	mat4 model = scene.modelMatrixes[gl_BaseInstance];
	outViewPos = vec3(views[gl_ViewID_OVR].viewMat * model * vec4(inPos, 1.0));
	gl_Position = views[gl_ViewID_OVR].projMat * vec4(outViewPos, 1.0);
}
