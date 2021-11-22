#version 450

#extension GL_OVR_multiview2 : enable
layout(num_views = 2) in;

#include "../lib/types_common.glsl"
#include "../lib/vertex_base.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 outViewPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outTexCoord;

layout(push_constant) uniform PushConstants {
	mat4 modelMat;
};

#include "lib/view_states_uniform.glsl"

void main() {
	ViewState view = views[gl_ViewID_OVR];

	vec4 playerPos = view.invViewMat * vec4(vec3(0.0), 1.0);
	vec4 worldPos = modelMat * vec4(inPosition, 1.0);
	float deltaPos = worldPos.x - playerPos.x;
	worldPos.y += (deltaPos * deltaPos) * 0.0035;

	vec4 viewPos4 = view.viewMat * worldPos;
	outViewPos = vec3(viewPos4) / viewPos4.w;
	gl_Position = view.projMat * viewPos4;

	mat3 rotation = mat3(view.viewMat * modelMat);
	outNormal = rotation * inNormal;
	outTexCoord = inTexCoord;
}
