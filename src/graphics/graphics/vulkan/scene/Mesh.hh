/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "assets/Gltf.hh"
#include "common/Async.hh"
#include "ecs/Ecs.hh"
#include "graphics/vulkan/core/Memory.hh"
#include "graphics/vulkan/core/VkCommon.hh"
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
            size_t jointsVertexOffset, jointsVertexCount;
            TextureHandle baseColor, metallicRoughness;
            glm::vec3 center = glm::vec3(0);
        };

        Mesh(shared_ptr<const sp::Gltf> source, size_t meshIndex, GPUScene &scene, DeviceContext &device);
        ~Mesh();

        uint32 SceneIndex() const;
        uint32 PrimitiveCount() const {
            return primitives.size();
        }
        uint32 IndexCount() const {
            return indexCount;
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

            if (!staging.transferComplete->Ready()) return false;

            ready = true;
            return true;
        }

    private:
        string modelName;
        shared_ptr<const sp::Gltf> asset;

        vector<Primitive> primitives;

        uint32 vertexCount = 0, indexCount = 0, jointsCount = 0;
        struct {
            BufferPtr indexBuffer, vertexBuffer, jointsBuffer, primitiveList, modelEntry;
            AsyncPtr<void> transferComplete;
        } staging;

        SubBufferPtr indexBuffer, vertexBuffer, jointsBuffer, primitiveList, modelEntry;

        bool ready = false;

        friend class GPUScene;
    };
} // namespace sp::vulkan
