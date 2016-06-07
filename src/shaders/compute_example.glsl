#version 430

layout(binding = 0) writeonly uniform image2D target;

layout(local_size_x = 16, local_size_y = 16) in;

void main() {
	imageStore(target, ivec2(gl_GlobalInvocationID.xy), vec4(vec2(gl_LocalInvocationID.xy) / 16.0, 1.0, 1.0));
}