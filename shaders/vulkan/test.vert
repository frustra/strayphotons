#version 450

#extension GL_OVR_multiview2 : enable
layout(num_views = 2) in;

#include "../lib/types_common.glsl"
#include "../lib/vertex_base.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 viewPosition;
layout(location = 1) out vec3 viewNormal;
layout(location = 2) out vec3 color;
layout(location = 3) out vec2 outTexCoord;

layout(push_constant) uniform PushConstants {
	mat4 modelMat;
};

#include "lib/view_states_uniform.glsl"

void main() {
	ViewState view = views[gl_ViewID_OVR];
	vec4 viewPos4 = view.viewMat * modelMat * vec4(inPosition, 1.0);
	viewPosition = vec3(viewPos4) / viewPos4.w;
	gl_Position = view.projMat * viewPos4;

	mat3 rotation = mat3(view.viewMat * modelMat);
	viewNormal = rotation * inNormal;
	outTexCoord = inTexCoord;
}
