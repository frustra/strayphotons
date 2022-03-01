#pragma once

#include "assets/Async.hh"
#include "assets/Gltf.hh"
#include "ecs/Ecs.hh"
#include "graphics/vulkan/core/Common.hh"
#include "graphics/vulkan/core/Memory.hh"
#include "graphics/vulkan/scene/GPUScene.hh"

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
            TextureHandle baseColor, metallicRoughness;
        };

        Mesh(shared_ptr<const sp::Gltf> source, size_t meshIndex, GPUScene &scene, DeviceContext &device);
        ~Mesh();

        uint32 SceneIndex() const;
        uint32 PrimitiveCount() const {
            return primitives.size();
        }
        uint32 VertexCount() const {
            return vertexCount;
        }

        bool CheckReady() {
            if (ready) return true;

            for (auto &prim : primitives) {
                if (!prim.baseColor.Ready()) return false;
                if (!prim.metallicRoughness.Ready()) return false;
            }

            ready = true;
            return true;
        }

    private:
        string modelName;
        shared_ptr<const sp::Gltf> asset;

        vector<Primitive> primitives;

        uint32 vertexCount = 0, indexCount = 0;
        SubBufferPtr indexBuffer, vertexBuffer, primitiveList, modelEntry;

        bool ready = false;
    };
} // namespace sp::vulkan
