#pragma once

#include "Common.hh"
#include "Memory.hh"
#include "ecs/Ecs.hh"

#include <functional>
#include <glm/glm.hpp>
#include <robin_hood.h>
#include <vulkan/vulkan.hpp>

namespace sp {
    class Model;
} // namespace sp

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
            UniqueBuffer indexBuffer;
            vk::IndexType indexType = vk::IndexType::eNoneKHR;
            uint32_t indexCount;

            UniqueBuffer vertexBuffer;
            glm::mat4 transform;
        };

        Model(const sp::Model &model, DeviceContext &device);
        ~Model();

        void AppendDrawCommands(CommandContext &commands, glm::mat4 modelMat, const ecs::View &view);

    private:
        vector<shared_ptr<Primitive>> primitives;
        string modelName;
    };
} // namespace sp::vulkan
