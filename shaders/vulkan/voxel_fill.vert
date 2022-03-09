#version 460
#extension GL_EXT_shader_16bit_storage : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require
#extension GL_OVR_multiview2 : enable
layout(num_views = 3) in;

#include "../lib/types_common.glsl"
#include "../lib/util.glsl"
#include "../lib/vertex_base.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 outViewPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outTexCoord;
layout(location = 3) flat out int baseColorTexID;
layout(location = 4) flat out int metallicRoughnessTexID;

#include "lib/draw_params.glsl"
layout(std430, set = 1, binding = 0) readonly buffer DrawParamsList {
    DrawParams drawParams[];
};

layout(binding = 0) uniform ViewStates {
    ViewState views[3];
};

void main() {
    ViewState view = views[gl_ViewID_OVR];
    gl_Position = view.viewMat * vec4(inPosition, 1.0);
    outViewPos = gl_Position.xyz / gl_Position.w;

    mat3 rotation = mat3(view.viewMat);
    outNormal = rotation * inNormal;
    outTexCoord = inTexCoord;

    DrawParams params = drawParams[gl_BaseInstance];
    baseColorTexID = int(params.baseColorTexID);
    metallicRoughnessTexID = int(params.metallicRoughnessTexID);
}
