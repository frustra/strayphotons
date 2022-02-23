#define SMAA_GLSL_4 1
#define SMAA_PRESET_HIGH 1

layout(push_constant) uniform PushConstants {
	vec4 smaaRTMetrics;
};
#define SMAA_RT_METRICS smaaRTMetrics

#include "smaa.glsl"
