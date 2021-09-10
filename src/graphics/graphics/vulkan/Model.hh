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
        glm::mat4 view;
        glm::mat4 projection;
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

        void AppendDrawCommands(const CommandContextPtr &commands, glm::mat4 modelMat, const ecs::View &view);

    private:
        ImageViewPtr LoadTexture(DeviceContext &device, const sp::Model &model, int materialIndex, TextureType type);

        robin_hood::unordered_map<string, ImageViewPtr> textures;
        vector<shared_ptr<Primitive>> primitives;
        string modelName;
    };
} // namespace sp::vulkan
