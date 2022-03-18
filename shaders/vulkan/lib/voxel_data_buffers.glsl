#extension GL_EXT_shader_16bit_storage : require

#include "indirect_commands.glsl"

struct VoxelFragment {
    u16vec3 position;
    f16vec3 radiance;
};

// layout(std430, set = 3, binding = 0) buffer VoxelFragmentList {
//     uint count;
//     uint capacity;
//     VkDispatchIndirectCommand cmd;
//     VoxelFragment list[];
// }
// fragmentLists[];
