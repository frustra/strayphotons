#version 450

layout(location = 0) in vec2 inPos;

layout(push_constant) uniform PushConstants {
    mat4 projMat;
}
constants;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    gl_Position = constants.projMat * vec4(inPos, 0.0, 1.0);
}
