#pragma once

#include "assets/Async.hh"
#include "assets/Gltf.hh"
#include "ecs/Ecs.hh"
#include "graphics/vulkan/GPUSceneContext.hh"
#include "graphics/vulkan/core/Common.hh"
#include "graphics/vulkan/core/Memory.hh"

#include <atomic>
#include <functional>
#include <future>
#include <glm/glm.hpp>
#include <robin_hood.h>
#include <vulkan/vulkan.hpp>

namespace sp::vulkan {
    struct MeshPushConstants {
        glm::mat4 model;
    };

    class Mesh final : public NonCopyable {
    public:
        struct Primitive {
            size_t indexOffset, indexCount;
            size_t vertexOffset, vertexCount;
            TextureIndex baseColor, metallicRoughness;
        };

        Mesh(shared_ptr<const sp::Gltf> source, size_t meshIndex, GPUSceneContext &scene, DeviceContext &device);
        ~Mesh();

        uint32 SceneIndex() const;
        uint32 PrimitiveCount() const {
            return primitives.size();
        }
        uint32 VertexCount() const {
            return vertexCount;
        }

        bool CheckReady() {
            if (pendingWork.empty()) return true;
            erase_if(pendingWork, [](auto &fut) {
                return fut->Ready();
            });
            return pendingWork.empty();
        }

    private:
        TextureIndex LoadTexture(DeviceContext &device,
            const shared_ptr<const sp::Gltf> &source,
            int materialIndex,
            TextureType type);
        string modelName;
        GPUSceneContext &scene;
        shared_ptr<const sp::Gltf> asset;
        size_t meshIndex;

        robin_hood::unordered_map<string, TextureIndex> textures;
        vector<Primitive> primitives;

        uint32 vertexCount = 0, indexCount = 0;
        SubBufferPtr indexBuffer, vertexBuffer, primitiveList, modelEntry;

        std::vector<AsyncPtr<void>> pendingWork;
    };
} // namespace sp::vulkan
