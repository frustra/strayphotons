#version 450

#extension GL_OVR_multiview2 : enable
layout(num_views = 2) in;

#include "../lib/util.glsl"
#include "../lib/types_common.glsl"

layout(binding = 0) uniform sampler2DArray luminanceBuffer;
layout(binding = 1) uniform sampler2DArray gBuffer2;

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outFragColor;

#include "lib/view_states_uniform.glsl"

struct LaserLine {
	vec3 color;
	vec3 start, end;
};

layout(std430, set = 0, binding = 9) readonly buffer LaserData {
	LaserLine laserLines[];
};

layout(push_constant) uniform PushConstants {
	uint laserLineCount;
};

const vec3 nearPlane = vec3(0, 0, -1);

void main() {
	ViewState view = views[gl_ViewID_OVR];

	vec4 luminance = texture(luminanceBuffer, vec3(inTexCoord, gl_ViewID_OVR));
	outFragColor = luminance;

	vec3 viewPos = texture(gBuffer2, vec3(inTexCoord, gl_ViewID_OVR)).rgb;
	vec4 fragPos = view.projMat * vec4(viewPos, 1);
	float depth = fragPos.z / fragPos.w;
	if (depth < 0) depth = 1;

	vec2 clipPos = vec2(inTexCoord.x * 2 - 1, 1 - inTexCoord.y * 2);

	for (uint i = 0; i < laserLineCount; i++) {
		vec4 startV = view.viewMat * vec4(laserLines[i].start, 1);
		vec4 endV = view.viewMat * vec4(laserLines[i].end, 1);

		if (startV.z > 0 && endV.z > 0) continue;

		// clip to near plane
		vec4 dirV = normalize(endV - startV);
		float dNearPlane = dot(vec3(0, 0, -0.01) - startV.xyz, nearPlane) / dot(dirV.xyz, nearPlane);
		if (startV.z > 0) startV = startV + dirV * dNearPlane;
		else if (endV.z > 0) endV = startV + dirV * dNearPlane;

		vec4 startP4 = view.projMat * startV;
		vec3 startP = startP4.xyz / startP4.w;
		vec4 endP4 = view.projMat * endV;
		vec3 endP = endP4.xyz / endP4.w;

		// get distance to laser line in 2D clip space
		// everything in the loop before this could be done on the CPU
		vec2 v = endP.xy - startP.xy, u = startP.xy - clipPos;
		float t = -dot(v, u) / dot(v, v);
		t = clamp(t, 0, 1);

		vec3 pos = startP * (1 - t) + endP * t;
		if (pos.z > depth) continue;

		float dist = distance(clipPos, pos.xy);

		float weight = 0.0001/(pow(dist, 2) + 0.00001) * max(1 - pos.z, 0);
		outFragColor.rgb += laserLines[i].color * weight;
	}
}
