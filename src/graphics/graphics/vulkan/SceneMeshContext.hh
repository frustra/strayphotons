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

    struct SceneMeshContext {
        BufferPtr indexBuffer;
        BufferPtr vertexBuffer;
        BufferPtr primitiveLists;
        BufferPtr models;

        uint32 renderableCount = 0;
        BufferPtr renderableEntityList;

        uint32 primitiveCount = 0;
        uint32 primitiveCountPowerOfTwo;

        vector<ImageViewPtr> textures;
        vector<TextureIndex> freeTextureIndexes;
        vector<TextureIndex> texturesToFlush;
        vk::DescriptorSet textureDescriptorSet;

        TextureIndex AddTexture(const ImageViewPtr &ptr) {
            TextureIndex i;
            if (!freeTextureIndexes.empty()) {
                i = freeTextureIndexes.back();
                freeTextureIndexes.pop_back();
                textures[i] = ptr;
            } else {
                i = textures.size();
                textures.push_back(ptr);
            }
            texturesToFlush.push_back(i);
            return i;
        }

        void ReleaseTexture(TextureIndex i) {
            textures[i].reset();
            freeTextureIndexes.push_back(i);
        }
    };
} // namespace sp::vulkan
