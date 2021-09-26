#version 450

layout(location = 0) out vec2 outTexCoord;

out gl_PerVertex
{
	vec4 gl_Position;
};

vec2 positions[3] = vec2[](
    vec2(-2, -1),
    vec2(2, -1),
    vec2(0, 3)
);

vec2 uvs[3] = vec2[](
    vec2(-0.5, 1),
    vec2(1.5, 1),
    vec2(0.5, -1)
);

const bool flipped = false;

void main() {
	outTexCoord = uvs[gl_VertexIndex];
    if (flipped) outTexCoord.y = 1 - outTexCoord.y;
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
}
