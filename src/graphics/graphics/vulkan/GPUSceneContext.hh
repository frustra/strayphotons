#pragma once

#include "graphics/vulkan/core/Common.hh"
#include "graphics/vulkan/core/Memory.hh"

namespace sp::vulkan {
    typedef uint32 TextureIndex;

    struct GPUViewState {
        GPUViewState() {}
        GPUViewState(const ecs::View &view) {
            projMat = view.projMat;
            invProjMat = view.invProjMat;
            viewMat = view.viewMat;
            invViewMat = view.invViewMat;
            clip = view.clip;
            extents = view.extents;
        }

        glm::mat4 projMat, invProjMat;
        glm::mat4 viewMat, invViewMat;
        glm::vec2 clip, extents;
    };
    static_assert(sizeof(GPUViewState) % 16 == 0, "std140 alignment");

    struct GPUMeshPrimitive {
        glm::mat4 primitiveToModel;
        uint32 firstIndex, vertexOffset;
        uint32 indexCount, vertexCount; // count of elements in the index/vertex buffers
        uint16 baseColorTexID, metallicRoughnessTexID;
        float _padding[3];
        // other material properties of the primitive can be stored here (or material ID)
    };
    static_assert(sizeof(GPUMeshPrimitive) % sizeof(glm::vec4) == 0, "std430 alignment");

    struct GPUMeshModel {
        uint32 primitiveOffset;
        uint32 primitiveCount;
        uint32 indexOffset;
        uint32 vertexOffset;
    };
    static_assert(sizeof(GPUMeshModel) % sizeof(uint32) == 0, "std430 alignment");

    struct GPURenderableEntity {
        glm::mat4 modelToWorld;
        uint32 modelIndex;
        uint32 visibilityMask;
        uint32 vertexOffset;
        float _padding[1];
    };
    static_assert(sizeof(GPURenderableEntity) % sizeof(glm::vec4) == 0, "std430 alignment");

    class GPUSceneContext {
    public:
        GPUSceneContext(DeviceContext &device);

        BufferPtr indexBuffer;
        BufferPtr vertexBuffer;
        BufferPtr primitiveLists;
        BufferPtr models;

        uint32 renderableCount = 0;
        BufferPtr renderableEntityList;

        uint32 vertexCount = 0;
        uint32 primitiveCount = 0;
        uint32 primitiveCountPowerOfTwo = 1; // Always at least 1. Used to size draw command buffers.

        TextureIndex AddTexture(const ImageViewPtr &ptr);
        void ReleaseTexture(TextureIndex i);
        void FlushTextureDescriptors();

        ImageViewPtr GetTexture(TextureIndex i) const {
            return textures[i];
        }

        vk::DescriptorSet GetTextureDescriptorSet() const {
            return textureDescriptorSet;
        }

    private:
        DeviceContext &device;

        vector<ImageViewPtr> textures;
        vector<TextureIndex> freeTextureIndexes;
        vector<TextureIndex> texturesToFlush;
        vk::DescriptorSet textureDescriptorSet;
    };
} // namespace sp::vulkan
