#version 450

layout(binding = 0) uniform sampler2D tex;
layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outFragColor;

void main() {
	float value = texture(tex, inTexCoord).r;
	outFragColor = vec4(vec3(value), 1);
}
