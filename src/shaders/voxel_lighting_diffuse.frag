#version 430

layout (binding = 0) uniform sampler2D gBuffer0;
layout (binding = 1) uniform sampler2D gBuffer1;
layout (binding = 2) uniform sampler2D depthStencil;

layout (binding = 3) uniform sampler3D voxelColor;
layout (binding = 4) uniform sampler3D voxelNormal;
layout (binding = 5) uniform sampler3D voxelAlpha;
layout (binding = 6) uniform sampler3D voxelRadiance;

layout (location = 0) in vec2 inTexCoord;
layout (location = 0) out vec4 outFragColor;

uniform float voxelSize = 0.1;
uniform vec3 voxelGridCenter = vec3(0);

uniform float exposure = 1.0;
uniform vec2 targetSize;

##import lib/util
##import voxel_shared
##import voxel_trace_shared

uniform mat4 invViewMat;
uniform mat4 invProjMat;

const int diffuseAngles = 6;
const float diffuseScale = 1.0 / diffuseAngles;


void main()
{
	// Determine normal of surface at this fragment.
	vec4 gb1 = texture(gBuffer1, inTexCoord);
	vec3 viewNormal = gb1.rgb;
	if (length(viewNormal) < 0.5) {
		// Normal not defined.
		outFragColor = vec4(0);
		return;
	}

	vec4 gb0 = texture(gBuffer0, inTexCoord);
	vec3 baseColor = gb0.rgb;
	float roughness = gb0.a;
	float metalness = gb1.a;

	// Determine coordinates of fragment.
	float depth = texture(depthStencil, inTexCoord).r;
	vec3 viewPosition = ScreenPosToViewPos(inTexCoord, depth, invProjMat);
	vec3 worldPosition = (invViewMat * vec4(viewPosition, 1.0)).xyz;

	vec3 worldNormal = mat3(invViewMat) * viewNormal;

	// Trace.
	vec4 indirectDiffuse = vec4(0);
	vec3 directDiffuseColor = baseColor - baseColor * metalness;

	for (float r = 0; r < diffuseAngles; r++) {
		for (float a = 0.3; a <= 0.9; a += 0.3) {
			vec3 sampleDir = OrientByNormal(r / diffuseAngles * 6.28, a, worldNormal);
			vec4 sampleColor = ConeTraceGridDiffuse(worldPosition, sampleDir, worldNormal);

			indirectDiffuse += sampleColor * dot(sampleDir, worldNormal) * vec4(directDiffuseColor, 1.0);
		}
	}

	indirectDiffuse *= diffuseScale;
	outFragColor = vec4(indirectDiffuse.rgb * exposure, 1.0);
}

