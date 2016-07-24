#version 430

#define USE_BUMP_MAP

##import lib/util

layout (binding = 0) uniform sampler2D baseColorTex;
layout (binding = 1) uniform sampler2D roughnessTex;
layout (binding = 2) uniform sampler2D bumpTex;

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inTangent;
layout (location = 2) in vec3 inBitangent;
layout (location = 3) in vec2 inTexCoord;

layout (location = 0) out vec4 gBuffer0;
layout (location = 1) out vec4 gBuffer1;

const float bumpDepth = 0.09;

void main()
{
	vec4 baseColor = texture(baseColorTex, inTexCoord);
	if (baseColor.a < 0.5) discard;

	float roughness = texture(roughnessTex, inTexCoord).r;

#ifdef USE_BUMP_MAP
	vec2 dCoord = 1.0 / textureSize(bumpTex, 0);

	mat3 tangentMat = mat3(normalize(inTangent), normalize(inBitangent), normalize(inNormal));
	// Some model faces have undefined UV coordiantes, ignore them.
	if (any(isnan(inTangent))) discard;

	float a = texture(bumpTex, inTexCoord).r;
	float dx;
	// If the UV coordinates are mirroed, calculate dx in the opposite direction.
	if (dot(cross(inNormal, inTangent), inBitangent) < 0) {
		dx = texture(bumpTex, inTexCoord + vec2(dCoord.x, 0)).r - a;
	} else {
		dx = a - texture(bumpTex, inTexCoord - vec2(dCoord.x, 0)).r;
	}
	float dy = a - texture(bumpTex, inTexCoord - vec2(0, dCoord.y)).r;

	vec3 viewNormal = normalize(tangentMat * vec3(dx, dy, bumpDepth));
#else
	vec3 viewNormal = normalize(inNormal);
#endif

	gBuffer0.rgb = baseColor.rgb;
	gBuffer0.a = roughness;
	gBuffer1.rgb = viewNormal;
	gBuffer1.a = 1.0;
}
