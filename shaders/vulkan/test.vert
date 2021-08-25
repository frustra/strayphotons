#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

out gl_PerVertex {
    vec4 gl_Position;
};
layout(location = 0) out vec3 color;

layout(push_constant) uniform PushConstants {
    mat4 transform;
} constants;

void main() {
    gl_Position = constants.transform * vec4(inPosition, 1.0);
    color = vec3(inTexCoord, 1.0);//(inNormal + vec3(1)) * 0.5;
}
