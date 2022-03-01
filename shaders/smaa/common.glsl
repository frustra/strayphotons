#define SMAA_GLSL_4 1
#define SMAA_PRESET_HIGH 1

#include "../lib/types_common.glsl"

layout(binding = 10) 
#include "../vulkan/lib/view_states_uniform.glsl"

#define SMAA_RT_METRICS vec4(views[gl_ViewID_OVR].invExtents, views[gl_ViewID_OVR].extents)

#include "smaa.glsl"
