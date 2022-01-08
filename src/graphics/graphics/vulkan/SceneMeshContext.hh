#pragma once

#include "graphics/vulkan/core/Common.hh"
#include "graphics/vulkan/core/Memory.hh"

namespace sp::vulkan {
    struct GPUMeshPrimitive {
        glm::mat4 primitiveToModel;
        uint32 indexOffset, indexCount, vertexOffset;
        float _padding[1];
        // other material properties of the primitive can be stored here (or material ID)
    };
    static_assert(sizeof(GPUMeshPrimitive) % 16 == 0, "std140 alignment");

    struct GPUMeshModel {
        uint32 primitiveOffset;
        uint32 primitiveCount;
    };
    static_assert(sizeof(GPUMeshModel) == 8, "std140 alignment");

    struct GPURenderableEntity {
        glm::mat4 modelToWorld;
        uint32 modelIndex;
        uint32 visibilityMask;
        float _padding[2];
    };
    static_assert(sizeof(GPURenderableEntity) % 16 == 0, "std140 alignment");

    struct SceneMeshContext {
        BufferPtr indexBuffer;
        BufferPtr vertexBuffer;
        BufferPtr primitiveLists;
        BufferPtr models;

        uint32 renderableCount = 0;
        BufferPtr renderableEntityList;
    };
} // namespace sp::vulkan
