struct MeshPrimitive {
    uint firstIndex, vertexOffset;
    uint indexCount, vertexCount;
    uint jointsVertexOffset;
    uint16_t baseColorTexID, metallicRoughnessTexID;
};

struct MeshModel {
    uint primitiveOffset;
    uint primitiveCount;
    uint indexOffset;
    uint vertexOffset;
};

struct RenderableEntity {
    mat4 modelToWorld;
    uint modelIndex;
    uint visibilityMask;
    uint vertexOffset;
    uint jointPosesOffset;
    uint opticID;
    float emissiveScale;
};

struct SceneVertex {
    vec4 positionAndNormalX;
    vec2 normalYZ;
    vec2 uv;
};
