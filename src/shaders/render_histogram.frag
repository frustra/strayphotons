#version 430

##import lib/util

layout (binding = 0) uniform sampler2D luminanceTex;
layout (binding = 0, r32ui) uniform uimage2D histogram;

layout (location = 0) in vec2 inTexCoord;
layout (location = 0) out vec4 outFragColor;

#define HistogramBins 64

void main() {
	ivec2 size = textureSize(luminanceTex, 0);
	outFragColor = texture(luminanceTex, inTexCoord);

	int bin = int(floor(inTexCoord.x * HistogramBins));
	uint count = imageLoad(histogram, ivec2(bin, 0)).r;
	float ratio = count / float(size.x * size.y);

	outFragColor.rgb += step(inTexCoord.y, ratio) * vec3(1, 0, 0);
	//outFragColor.rgb = vec3(float(bin) / 32);
}
