#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

out gl_PerVertex {
    vec4 gl_Position;
};

layout(location = 0) out vec3 viewPos;
layout(location = 1) out vec3 viewNormal;
layout(location = 2) out vec3 color;

layout(push_constant) uniform PushConstants {
    mat4 model;
    mat4 view;
    mat4 projection;
} constants;

void main() {
    vec4 viewPos4 = constants.view * constants.model * vec4(inPosition, 1.0);
    viewPos = vec3(viewPos4) / viewPos4.w;
    gl_Position = constants.projection * viewPos4;

    mat3 rotation = mat3(constants.view * constants.model);
    viewNormal = rotation * inNormal;

    color = (viewNormal + vec3(1)) * 0.5;
}
