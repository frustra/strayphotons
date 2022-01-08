#pragma once

#include "assets/Model.hh"
#include "ecs/Ecs.hh"
#include "graphics/vulkan/SceneMeshContext.hh"
#include "graphics/vulkan/core/Common.hh"
#include "graphics/vulkan/core/Memory.hh"

#include <functional>
#include <glm/glm.hpp>
#include <robin_hood.h>
#include <vulkan/vulkan.hpp>

namespace sp::vulkan {
    struct MeshPushConstants {
        glm::mat4 model;
    };

    class Model final : public NonCopyable {
    public:
        struct Primitive : public NonCopyable {
            SubBufferPtr indexBuffer;
            vk::IndexType indexType = vk::IndexType::eNoneKHR;
            size_t indexCount;

            SubBufferPtr vertexBuffer;
            glm::mat4 transform;

            ImageViewPtr baseColor, metallicRoughness;
        };

        Model(const sp::Model &model, SceneMeshContext &scene, DeviceContext &device);
        ~Model();

        void Draw(CommandContext &cmd, SceneMeshContext &scene, glm::mat4 modelMat, bool useMaterial = true);
        uint32 SceneIndex() const;

    private:
        ImageViewPtr LoadTexture(DeviceContext &device, const sp::Model &model, int materialIndex, TextureType type);
        string modelName;

        robin_hood::unordered_map<string, ImageViewPtr> textures;
        vector<shared_ptr<Primitive>> primitives;

        SubBufferPtr primitiveList, modelEntry;
    };
} // namespace sp::vulkan
