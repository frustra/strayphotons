#version 430

#extension GL_OVR_multiview2 : enable
layout(num_views = 1) in;

#include "../lib/vertex_base.glsl"
#include "../lib/types_common.glsl"

layout(location = 0) in vec3 inPos;
layout(location = 0) out vec3 outViewPos;

layout(push_constant) uniform PushConstants {
	mat4 model;
} constants;

layout(binding = 10) uniform ViewStates {
	ViewState views[2];
};

void main() {
	outViewPos = vec3(views[gl_ViewID_OVR].viewMat * constants.model * vec4(inPos, 1.0));
	gl_Position = views[gl_ViewID_OVR].projMat * vec4(outViewPos, 1.0);
}
