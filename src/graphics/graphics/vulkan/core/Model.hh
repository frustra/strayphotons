#pragma once

#include "assets/Model.hh"
#include "ecs/Ecs.hh"
#include "graphics/vulkan/GPUSceneContext.hh"
#include "graphics/vulkan/core/Common.hh"
#include "graphics/vulkan/core/Memory.hh"

#include <functional>
#include <future>
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
            glm::mat4 transform;
            vk::IndexType indexType = vk::IndexType::eNoneKHR;
            size_t indexOffset, indexCount;
            size_t vertexOffset, vertexCount;
            TextureIndex baseColor, metallicRoughness;
        };

        Model(const sp::Model &model, GPUSceneContext &scene, DeviceContext &device);
        ~Model();

        void Draw(CommandContext &cmd, glm::mat4 modelMat, bool useMaterial = true);
        uint32 SceneIndex() const;
        uint32 PrimitiveCount() const {
            return primitives.size();
        }
        uint32 VertexCount() const {
            return vertexCount;
        }

        bool Ready() {
            if (ready.empty()) return true;
            erase_if(ready, [](std::future<void> &fut) {
                return fut.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
            });
            return ready.empty();
        }

    private:
        TextureIndex LoadTexture(DeviceContext &device, const sp::Model &model, int materialIndex, TextureType type);
        string modelName;
        GPUSceneContext &scene;

        robin_hood::unordered_map<string, TextureIndex> textures;
        vector<shared_ptr<Primitive>> primitives;

        uint32 vertexCount = 0, indexCount = 0;
        SubBufferPtr indexBuffer, vertexBuffer, primitiveList, modelEntry;

        std::vector<std::future<void>> ready;
    };
} // namespace sp::vulkan
