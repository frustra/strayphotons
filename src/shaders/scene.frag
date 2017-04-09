#version 430

layout (early_fragment_tests) in; // Force stencil testing before shader invocation.

##import lib/util
##import lib/types_common
##import lib/mirror_scene_common

layout (binding = 0) uniform sampler2D baseColorTex;
layout (binding = 1) uniform sampler2D roughnessTex;
layout (binding = 2) uniform sampler2D metallicTex;
layout (binding = 3) uniform sampler2D heightTex;
layout (binding = 4) uniform usampler2D inMirrorIndexStencil;

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inTangent;
layout (location = 2) in vec3 inBitangent;
layout (location = 3) in vec2 inTexCoord;
layout (location = 4) in vec3 inViewPos;
layout (location = 5) flat in int inMirrorIndex;

layout (location = 0) out vec4 gBuffer0; // rgba8
layout (location = 1) out vec4 gBuffer1; // rgba16f
layout (location = 2) out vec4 gBuffer2; // rgba16f
layout (location = 3) out vec4 gBuffer3; // rgba8
layout (location = 4) out uint outMirrorIndexStencil;

const float bumpDepth = 0.1;

uniform int drawMirrorId;
uniform vec3 emissive = vec3(0);

void main()
{
	if (inMirrorIndex >= 0) {
		// Rendering from a mirror.
		// Discard fragments that are in an area not meant for this mirror.
		vec2 screenTexCoord = gl_FragCoord.xy / textureSize(inMirrorIndexStencil, 0).xy;
		uint mirrorIndexStencil = texture(inMirrorIndexStencil, screenTexCoord).x;

		// Value is offset by 1.
		if (mirrorIndexStencil != mirrorSData.list[inMirrorIndex] + 1) {
			discard;
		}

		outMirrorIndexStencil = mirrorIndexStencil;
	} else {
		outMirrorIndexStencil = 0;
	}

	if (drawMirrorId >= 0) {
		int sourceId = max(0, inMirrorIndex);
		mirrorSData.mask[sourceId][drawMirrorId] = 1;

		// Offset value by 1 to differentiate blank space.
		outMirrorIndexStencil = PackSourceAndMirror(sourceId, drawMirrorId, inMirrorIndex < 0) + 1;
		return;
	}

	vec4 baseColor = texture(baseColorTex, inTexCoord);
	if (baseColor.a < 0.5) discard;

	float roughness = texture(roughnessTex, inTexCoord).r;
	float metallic = texture(metallicTex, inTexCoord).r;

#ifdef BUMP_MAP_ENABLED
	vec2 dCoord = 1.0 / textureSize(heightTex, 0);

	mat3 tangentMat = mat3(normalize(inTangent), normalize(inBitangent), normalize(inNormal));

	float a = texture(heightTex, inTexCoord).r;
	vec3 tangetNormal = vec3(0, 0, bumpDepth);

	// Fix aliasing on the edge of objects
	if (all(greaterThan(abs(fract(inTexCoord - 0.5) - 0.5), dCoord * 2.0))) {
		// If the UV coordinates are mirrored, calculate dx in the opposite direction.
		float reversed = step(0, dot(cross(inNormal, inTangent), inBitangent)) * 2.0 - 1.0;
		tangetNormal.x = reversed * (a - texture(heightTex, inTexCoord - vec2(reversed * dCoord.x, 0)).r);
		tangetNormal.y = a - texture(heightTex, inTexCoord - vec2(0, dCoord.y)).r;
	}

	vec3 viewNormal = normalize(tangentMat * tangetNormal);

	// Some model faces have undefined UV coordiantes, ignore bump map.
	if (any(isnan(inTangent))) viewNormal = inNormal;
#else
	vec3 viewNormal = inNormal;
#endif

	float emissiveScale = max(emissive.r, max(emissive.g, emissive.b));
	vec3 emissiveCoeff = emissiveScale > 0 ? emissive / emissiveScale : vec3(1.0f);

	gBuffer0.rgb = baseColor.rgb * emissiveCoeff;
	gBuffer0.a = roughness;
	gBuffer1.rg = EncodeNormal(viewNormal);
	gBuffer1.ba = EncodeNormal(inNormal);
	gBuffer2.rgb = inViewPos;
	gBuffer2.a = emissiveScale;
	gBuffer3.r = metallic;
}
