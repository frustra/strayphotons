struct MeshPrimitive {
	mat4 primitiveToModel;
	uint firstIndex, vertexOffset;
	uint indexCount, vertexCount;
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
};

struct SceneVertex {
	vec4 positionAndNormalX;
	vec2 normalYZ;
	vec2 uv;
};
