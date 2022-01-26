struct VkDrawIndirectCommand {
    uint vertexCount;
    uint instanceCount;
    uint firstVertex;
    uint firstInstance;
};

struct VkDrawIndexedIndirectCommand {
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int vertexOffset;
    uint firstInstance;
};
