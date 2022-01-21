#version 460
#extension GL_EXT_shader_16bit_storage : require
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
layout(location = 3) out int outDrawID;

#include "lib/draw_params.glsl"
layout(std430, set = 1, binding = 0) readonly buffer DrawParamsList {
	DrawParams drawParams[];
};

#include "lib/view_states_uniform.glsl"

void main() {
	mat4 modelMat = drawParams[gl_BaseInstance].modelMat;
	ViewState view = views[gl_ViewID_OVR];
	vec4 viewPos4 = view.viewMat * modelMat * vec4(inPosition, 1.0);
	outViewPos = vec3(viewPos4) / viewPos4.w;
	gl_Position = view.projMat * viewPos4;

	mat3 rotation = mat3(view.viewMat * modelMat);
	outNormal = rotation * inNormal;
	outTexCoord = inTexCoord;
	outDrawID = gl_BaseInstance;
}