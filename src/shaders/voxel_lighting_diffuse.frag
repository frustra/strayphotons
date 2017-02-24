#version 430

layout (binding = 0) uniform sampler2D gBuffer0;
layout (binding = 1) uniform sampler2D gBuffer1;
layout (binding = 2) uniform sampler2D gBuffer2;
layout (binding = 3) uniform sampler2D gBuffer3;

layout (binding = 4) uniform sampler3D voxelColor;
layout (binding = 5) uniform sampler3D voxelNormal;
layout (binding = 6) uniform sampler3D voxelRadiance;

layout (location = 0) in vec2 inTexCoord;
layout (location = 0) out vec4 outFragColor;

uniform float voxelSize = 0.1;
uniform vec3 voxelGridCenter = vec3(0);

uniform float exposure = 1.0;

##import lib/util
##import voxel_shared
##import voxel_trace_shared

uniform mat4 invViewMat;

void main()
{
	vec4 gb0 = texture(gBuffer0, inTexCoord);
	vec4 gb1 = texture(gBuffer1, inTexCoord);
	vec4 gb3 = texture(gBuffer3, inTexCoord);

	vec3 baseColor = gb0.rgb;
	float roughness = gb0.a;
	vec3 viewNormal = gb1.rgb;
	vec3 viewPosition = gb3.rgb;
	vec3 worldPosition = (invViewMat * vec4(viewPosition, 1.0)).xyz;
	vec3 worldNormal = mat3(invViewMat) * viewNormal;

	// Trace.
	vec3 indirectDiffuse = HemisphereIndirectDiffuse(worldPosition, worldNormal, gl_FragCoord.xy);

	outFragColor = vec4(indirectDiffuse.rgb * exposure, 1.0);
}

