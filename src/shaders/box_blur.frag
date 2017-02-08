#version 430

layout (binding = 0) uniform sampler2D tex;

layout (location = 0) in vec2 inTexCoord;

layout (location = 0) out vec4 outFragColor;

void main() {
	ivec2 pos = ivec2(inTexCoord * textureSize(tex, 0));

	vec3 sum = vec3(0);
	for (int x = -1; x <= 1; x++) {
		for (int y = -1; y <= 1; y++) {
			sum += texelFetch(tex, pos + ivec2(x, y), 0).rgb;
		}
	}
	outFragColor.rgb = sum / 9.0;
}