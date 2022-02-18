#version 450

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec4 inColor;

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    mat4 projMat;
}
constants;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    outTexCoord = inTexCoord;
    outColor = inColor;
    gl_Position = constants.projMat * vec4(inPos, 0.0, 1.0);
}
