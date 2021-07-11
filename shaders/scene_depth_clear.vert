#version 430

##import lib/vertex_base

layout (location = 0) in vec3 position;

void main() {
	gl_Position = vec4(position.xy, 1, 1); // far plane in clip space
}