#pragma once

#include "Common.hh"
#include "Memory.hh"
#include "assets/Model.hh"
#include "ecs/Ecs.hh"

#include <functional>
#include <glm/glm.hpp>
#include <robin_hood.h>
#include <vulkan/vulkan.hpp>

namespace sp::vulkan {
    class DeviceContext;

    struct MeshPushConstants {
        glm::mat4 model;
    };

    class Model final : public NonCopyable {
    public:
        struct Primitive : public NonCopyable {
            BufferPtr indexBuffer;
            vk::IndexType indexType = vk::IndexType::eNoneKHR;
            size_t indexCount;

            BufferPtr vertexBuffer;
            glm::mat4 transform;

            ImageViewPtr baseColor, metallicRoughness;
        };

        Model(const sp::Model &model, DeviceContext &device);
        ~Model();

        void Draw(const CommandContextPtr &cmd, glm::mat4 modelMat);

    private:
        ImageViewPtr LoadTexture(DeviceContext &device, const sp::Model &model, int materialIndex, TextureType type);

        robin_hood::unordered_map<string, ImageViewPtr> textures;
        vector<shared_ptr<Primitive>> primitives;
        string modelName;
    };
} // namespace sp::vulkan
