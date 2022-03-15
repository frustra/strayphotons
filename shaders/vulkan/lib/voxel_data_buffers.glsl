#extension GL_EXT_shader_16bit_storage : require

#include "indirect_commands.glsl"

struct VoxelFragment {
    uint16_t position[3];
    float16_t radiance[3];
};

layout(std430, set = 3, binding = 0) buffer VoxelFragmentCounts {
    uint fragmentCount;
    uint overflowCount[3];
    VkDispatchIndirectCommand fragmentsCmd;
    VkDispatchIndirectCommand overflowCmd[3];
};

layout(std430, set = 3, binding = 1) buffer VoxelFragmentList {
    VoxelFragment fragmentList[];
};

layout(std430, set = 3, binding = 2) buffer VoxelOverflowList0 {
    VoxelFragment overflowList0[];
};

layout(std430, set = 3, binding = 3) buffer VoxelOverflowList1 {
    VoxelFragment overflowList1[];
};

layout(std430, set = 3, binding = 4) buffer VoxelOverflowList2 {
    VoxelFragment overflowList2[];
};
