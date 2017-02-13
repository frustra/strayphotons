#version 430

layout (binding = 0) uniform sampler2D tex;

layout (location = 0) in vec2 inTexCoord;

layout (location = 0) out vec4 outFragColor;

// const float[5] kernel = float[](0.153388, 0.221461, 0.250301, 0.221461, 0.153388);
const float[3] kernel = float[](0.27901, 0.44198, 0.27901);

const int radius = 1;

void main() {
	ivec2 pos = ivec2(inTexCoord * textureSize(tex, 0));

	vec4 sum = vec4(0);
	for (int x = -radius; x <= radius; x++) {
		for (int y = -radius; y <= radius; y++) {
			sum += texelFetch(tex, pos + ivec2(x, y), 0) * kernel[x + radius] * kernel[y + radius];
		}
	}
	outFragColor = sum;
}