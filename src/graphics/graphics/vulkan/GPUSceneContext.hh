#pragma once

#include "graphics/vulkan/core/Common.hh"
#include "graphics/vulkan/core/Memory.hh"

namespace sp::vulkan {
    typedef uint32 TextureIndex;

    struct GPUMeshPrimitive {
        glm::mat4 primitiveToModel;
        uint32 indexOffset, indexCount, vertexOffset;
        uint16_t baseColorTexID, metallicRoughnessTexID;
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

    class GPUSceneContext {
    public:
        GPUSceneContext(DeviceContext &device);

        BufferPtr indexBuffer;
        BufferPtr vertexBuffer;
        BufferPtr primitiveLists;
        BufferPtr models;

        uint32 renderableCount = 0;
        BufferPtr renderableEntityList;

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
