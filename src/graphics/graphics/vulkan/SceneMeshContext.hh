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
    static_assert(sizeof(GPUMeshPrimitive) % sizeof(glm::vec4) == 0, "std430 alignment");

    struct GPUMeshModel {
        uint32 primitiveOffset;
        uint32 primitiveCount;
    };
    static_assert(sizeof(GPUMeshModel) % sizeof(uint32) == 0, "std430 alignment");

    struct GPURenderableEntity {
        glm::mat4 modelToWorld;
        uint32 modelIndex;
        uint32 visibilityMask;
        float _padding[2];
    };
    static_assert(sizeof(GPURenderableEntity) % sizeof(glm::vec4) == 0, "std430 alignment");

    struct SceneMeshContext {
        BufferPtr indexBuffer;
        BufferPtr vertexBuffer;
        BufferPtr primitiveLists;
        BufferPtr models;

        uint32 renderableCount = 0;
        BufferPtr renderableEntityList;

        uint32 primitiveCount = 0;
        uint32 primitiveCountPowerOfTwo;
    };
} // namespace sp::vulkan
