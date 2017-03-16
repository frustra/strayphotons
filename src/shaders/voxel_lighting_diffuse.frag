#version 430

layout (binding = 0) uniform sampler2D gBuffer0;
layout (binding = 1) uniform sampler2D gBuffer1;
layout (binding = 2) uniform sampler2D gBuffer2;
layout (binding = 3) uniform sampler3D voxelRadiance;
layout (binding = 4) uniform sampler3D voxelRadianceMips;

layout (location = 0) in vec2 inTexCoord;
layout (location = 0) out vec4 outFragColor;

uniform float voxelSize = 0.1;
uniform vec3 voxelGridCenter = vec3(0);

uniform float exposure = 1.0;
uniform float diffuseDownsample = 1;

##import lib/util
##import voxel_shared
##import voxel_trace_shared

uniform mat4 invViewMat;

void main()
{
	vec4 gb0 = texture(gBuffer0, inTexCoord);
	vec4 gb1 = texture(gBuffer1, inTexCoord);
	vec4 gb2 = texture(gBuffer2, inTexCoord);

	vec3 baseColor = gb0.rgb;
	float roughness = gb0.a;
	vec3 viewNormal = DecodeNormal(gb1.rg);
	vec3 viewPosition = gb2.rgb;

	if (length(viewNormal) < 0.9) {
		outFragColor.rgb = vec3(0);
		return;
	}

	vec3 worldPosition = vec3(invViewMat * vec4(viewPosition, 1.0));
	vec3 worldNormal = mat3(invViewMat) * viewNormal;

	// Trace. Only randomly seed if diffuse downsampling is off.
	vec3 indirectDiffuse = HemisphereIndirectDiffuse(worldPosition, worldNormal, gl_FragCoord.xy * step(-1, -diffuseDownsample));

	outFragColor.rgb = indirectDiffuse.rgb * exposure;
}
