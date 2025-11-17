/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "assets/Gltf.hh"
#include "common/Async.hh"
#include "common/Hashing.hh"
#include "common/PreservingMap.hh"
#include "ecs/components/View.hh"
#include "graphics/vulkan/core/Image.hh"
#include "graphics/vulkan/core/Memory.hh"
#include "graphics/vulkan/core/VkCommon.hh"
#include "graphics/vulkan/render_graph/RenderGraph.hh"
#include "graphics/vulkan/scene/TextureSet.hh"

namespace sp::vulkan {
    class Mesh;

    struct GPUViewState {
        GPUViewState() {}
        GPUViewState(const ecs::View &view) {
            projMat = view.projMat;
            invProjMat = view.invProjMat;
            viewMat = view.viewMat;
            invViewMat = view.invViewMat;
            extents = view.extents;
            invExtents = 1.0f / extents;
            clip = view.clip;
        }

        glm::mat4 projMat, invProjMat;
        glm::mat4 viewMat, invViewMat;
        glm::vec2 extents, invExtents;
        glm::vec2 clip, _padding;
    };
    static_assert(sizeof(GPUViewState) % 16 == 0, "std140 alignment");

    struct GPUMeshPrimitive {
        uint32 firstIndex, vertexOffset;
        uint32 indexCount, vertexCount; // count of elements in the index/vertex buffers
        uint32 jointsVertexOffset;
        uint16 baseColorTexID, metallicRoughnessTexID;
        // other material properties of the primitive can be stored here (or material ID)
    };
    static_assert(sizeof(GPUMeshPrimitive) % sizeof(uint32) == 0, "std430 alignment");

    struct GPUMeshModel {
        uint32 primitiveOffset;
        uint32 primitiveCount;
        uint32 indexOffset;
        uint32 vertexOffset;
    };
    static_assert(sizeof(GPUMeshModel) % sizeof(uint32) == 0, "std430 alignment");

    struct GPURenderableEntity {
        glm::mat4 modelToWorld;
        uint32_t meshIndex;
        uint32_t visibilityMask;
        uint32_t vertexOffset;
        uint32_t jointPosesOffset = 0xffffffff;
        uint32_t opticID = 0;
        float emissiveScale = 0;
        int32_t baseColorOverrideID = -1;
        int32_t metallicRoughnessOverrideID = -1;
    };
    static_assert(sizeof(GPURenderableEntity) % sizeof(glm::vec4) == 0, "std430 alignment");

    struct GPUDrawParams {
        uint16_t baseColorTexID;
        uint16_t metallicRoughnessTexID;
        uint16_t opticID = 0;
        float16_t emissiveScale = 0.0f;
    };
    static_assert(sizeof(GPUDrawParams) % sizeof(uint16_t) == 0, "std430 alignment");

    class GPUScene {
    private:
        DeviceContext &device;

    public:
        GPUScene(DeviceContext &device);
        void Flush();
        void LoadState(rg::RenderGraph &graph,
            ecs::Lock<ecs::Read<ecs::Renderable, ecs::OpticalElement, ecs::TransformSnapshot, ecs::Name>> lock);
        bool PreloadTextures(
            ecs::Lock<ecs::Read<ecs::Name, ecs::Renderable, ecs::Light, ecs::RenderOutput, ecs::Screen>> lock);
        void AddGraphTextures(rg::RenderGraph &graph);
        shared_ptr<Mesh> LoadMesh(const std::shared_ptr<const sp::Gltf> &model, size_t meshIndex);

        struct DrawBufferIDs {
            rg::ResourceID drawCommandsBuffer; // first 4 bytes are the number of draws
            rg::ResourceID drawParamsBuffer = 0;
        };

        DrawBufferIDs GenerateDrawsForView(rg::RenderGraph &graph,
            ecs::VisibilityMask viewMask,
            uint32 instanceCount = 1);

        // Sort primitives nearest first by default.
        DrawBufferIDs GenerateSortedDrawsForView(rg::RenderGraph &graph,
            glm::vec3 viewPosition,
            ecs::VisibilityMask viewMask,
            bool reverseSort = false,
            uint32 instanceCount = 1);

        void DrawSceneIndirect(CommandContext &cmd,
            BufferPtr vertexBuffer,
            BufferPtr drawCommandsBuffer,
            BufferPtr drawParamsBuffer);

        void AddGeometryWarp(rg::RenderGraph &graph);

        BufferPtr indexBuffer;
        BufferPtr vertexBuffer;
        BufferPtr jointsBuffer;
        BufferPtr primitiveLists;
        BufferPtr models;

        struct OpticInstance {
            ecs::Entity ent;
            bool pass;
            bool reflect;

            OpticInstance(ecs::Entity ent, const ecs::OpticalElement &optic);

            bool operator==(const OpticInstance &) const = default;
        };

        uint32 renderableCount = 0;
        std::vector<OpticInstance> opticEntities;
        std::vector<glm::mat4> jointPoses;

        uint32 vertexCount = 0;
        uint32 primitiveCount = 0;
        uint32 primitiveCountPowerOfTwo = 1; // Always at least 1. Used to size draw command buffers.

        TextureSet textures;
        robin_hood::unordered_map<rg::ResourceName, TextureHandle, StringHash, StringEqual> textureCache;

    private:
        void FlushMeshes();
        struct MeshKey {
            rg::ResourceName modelName;
            size_t meshIndex;
        };

        struct MeshKeyView {
            std::string_view modelName;
            size_t meshIndex;
        };

        struct MeshKeyHash {
            using is_transparent = void;

            std::size_t operator()(const MeshKey &key) const {
                auto h = StringHash{}(key.modelName);
                hash_combine(h, key.meshIndex);
                return h;
            }
            std::size_t operator()(const MeshKeyView &key) const {
                auto h = StringHash{}(key.modelName);
                hash_combine(h, key.meshIndex);
                return h;
            }
        };

        struct MeshKeyEqual {
            using is_transparent = void;

            bool operator()(const MeshKeyView &lhs, const MeshKey &rhs) const {
                const std::string_view view = rhs.modelName;
                return lhs.modelName == view && lhs.meshIndex == rhs.meshIndex;
            }
            bool operator()(const MeshKey &lhs, const MeshKey &rhs) const {
                return lhs.modelName == rhs.modelName && lhs.meshIndex == rhs.meshIndex;
            }
        };

        struct AsyncMesh {
            std::shared_ptr<const sp::Gltf> gltf;
            AsyncPtr<Mesh> async;

            AsyncMesh(const std::shared_ptr<const sp::Gltf> &gltf, AsyncPtr<Mesh> async = nullptr)
                : gltf(gltf), async(async) {}
        };

        PreservingMap<MeshKey, Mesh, 10000, MeshKeyHash, MeshKeyEqual> activeMeshes;
        robin_hood::unordered_flat_map<MeshKey, AsyncMesh, MeshKeyHash, MeshKeyEqual> meshesToLoad;
        vector<GPURenderableEntity> renderables;
        vector<std::pair<rg::ResourceName, size_t>> renderableTextureOverrides;
        vector<std::weak_ptr<Mesh>> meshes;

        DispatchQueue workQueue;
    };
} // namespace sp::vulkan
