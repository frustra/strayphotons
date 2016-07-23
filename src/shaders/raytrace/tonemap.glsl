#version 430

layout (binding = 0) uniform sampler2D tex;

layout (location = 0) in vec2 inTexCoord;
layout (location = 0) out vec4 outFragColor;

vec3 FilmicTonemap(vec3 x) {
	// Uncharted 2 implementation of filmic tone mapping by Kodak [Hable 2010]

	const float A = 0.15;
	const float B = 0.50;
	const float C = 0.10;
	const float D = 0.20;
	const float E = 0.02;
	const float F = 0.30;

	return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

void main() {
	vec4 hdr = texture(tex, inTexCoord);
	vec3 toneMapped = FilmicTonemap(hdr.rgb) / FilmicTonemap(vec3(1.0));
	outFragColor = vec4(toneMapped, hdr.a);
}
