/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justine Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "assets/AssetManager.hh"
#include "assets/Gltf.hh"
#include "common/PreservingMap.hh"
#include "ecs/Ecs.hh"
#include "ecs/components/Light.hh"
#include "ecs/components/Renderable.hh"
#include "ecs/components/View.hh"
#include "graphics/vulkan/core/VkCommon.hh"
#include "graphics/vulkan/render_graph/RenderGraph.hh"
#include "graphics/vulkan/render_graph/Resources.hh"
#include "graphics/vulkan/scene/TextureSet.hh"
#include "strayphotons/Async.hh"
#include "strayphotons/EntityMap.hh"
#include "strayphotons/Hashing.hh"
#include "strayphotons/HeapVector.hh"
#include "strayphotons/InlineString.hh"

#include <cstdint>
#include <limits>
#include <memory>

namespace sp::vulkan {
    typedef uint64_t RenderableIndex;
    typedef uint32_t MeshIndex;
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
        uint32_t firstIndex, vertexOffset;
        uint32_t indexCount, vertexCount; // count of elements in the index/vertex buffers
        uint32_t jointsVertexOffset;
        uint16_t baseColorTexID, metallicRoughnessTexID;
        // other material properties of the primitive can be stored here (or material ID)
    };
    static_assert(sizeof(GPUMeshPrimitive) % sizeof(uint32_t) == 0, "std430 alignment");

    struct GPUMeshModel {
        uint32_t primitiveOffset;
        uint32_t primitiveCount;
        uint32_t indexOffset;
        uint32_t vertexOffset;
    };
    static_assert(sizeof(GPUMeshModel) % sizeof(uint32_t) == 0, "std430 alignment");

    struct GPURenderableEntity {
        glm::mat4 modelToWorld;
        MeshIndex meshIndex = std::numeric_limits<MeshIndex>::max();
        uint32_t visibilityMask = 0;
        uint32_t vertexOffset = ~0u;
        uint32_t jointPosesOffset = ~0u;
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
        ~GPUScene();
        void Flush();
        void LoadState(rg::RenderGraph &graph,
            ecs::Lock<ecs::Read<ecs::Renderable,
                ecs::VoxelData,
                ecs::Light,
                ecs::OpticalElement,
                ecs::TransformSnapshot,
                ecs::Name>> lock);
        bool PreloadScene(ecs::Lock<ecs::Read<ecs::Name,
                              ecs::SceneInfo,
                              ecs::Renderable,
                              ecs::VoxelData,
                              ecs::Light,
                              ecs::RenderOutput,
                              ecs::Screen>> lock,
            std::shared_ptr<Scene> scene);
        AsyncPtr<Mesh> LoadMesh(const ecs::Renderable &renderable);
        AsyncPtr<Mesh> LoadMesh(const ecs::VoxelData &voxelData);
        AsyncPtr<Gltf> GenerateGltf(const ecs::VoxelData &voxelData);

        struct DrawBufferIDs {
            rg::ResourceID drawCommandsBuffer = rg::InvalidResource; // first 4 bytes are the number of draws
            rg::ResourceID drawParamsBuffer = rg::InvalidResource;

            operator bool() const {
                return drawCommandsBuffer != rg::InvalidResource && drawParamsBuffer != rg::InvalidResource;
            }
        };

        DrawBufferIDs GenerateDrawsForView(rg::RenderGraph &graph,
            ecs::VisibilityMask viewMask,
            uint32_t instanceCount = 1);

        // Sort primitives nearest first by default.
        DrawBufferIDs GenerateSortedDrawsForView(rg::RenderGraph &graph,
            glm::vec3 viewPosition,
            ecs::VisibilityMask viewMask,
            bool reverseSort = false,
            uint32_t instanceCount = 1);

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

        HeapVector<OpticInstance> opticEntities;
        HeapVector<glm::mat4> jointPoses;

        uint32_t vertexCount = 0;
        uint32_t primitiveCount = 0;
        uint32_t primitiveCountPowerOfTwo = 0;

        TextureSet textures;

        struct EntityState {
            AssetName renderableModelName;
            RenderableIndex renderableIndex = std::numeric_limits<RenderableIndex>::max();
            MeshIndex renderableMeshIndex = std::numeric_limits<MeshIndex>::max();
            AsyncPtr<Mesh> renderableMesh, voxelDataMesh;
            HeapString voxelDataAlgorithm;
            glm::uvec3 voxelDataExtents = glm::uvec3(0);
            uint64_t voxelDataSeed = 0;
            rg::ResourceName lightFilterName, renderableTextureOverrideName;
            TextureHandle lightFilter, renderableTextureOverride;

            EntityState() {}
            bool operator==(const EntityState &) const = default;
        };
        EntityMap<EntityState> liveEntityState, stagingEntityState;

    private:
        ecs::ComponentModifiedObserver<ecs::Renderable> renderableObserver;
        ecs::ComponentModifiedObserver<ecs::VoxelData> voxelDataObserver;
        ecs::ComponentModifiedObserver<ecs::Light> lightObserver;

        void FlushMeshes();
        struct MeshKey {
            rg::ResourceName modelName;
            MeshIndex meshIndex;
        };

        struct MeshKeyView {
            std::string_view modelName;
            MeshIndex meshIndex;
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

        PreservingMap<AssetName, Async<Gltf>, 10000, StringHash, StringEqual> activeModels;
        PreservingMap<MeshKey, Async<Mesh>, 10000, MeshKeyHash, MeshKeyEqual> activeMeshes;

        RenderableIndex AllocateRenderableIndex(ecs::Entity ent);
        void ReleaseRenderable(RenderableIndex index);

        struct VoxelDataKey {
            sp::InlineString<128> algorithm;
            glm::uvec3 extents;
            uint32_t seed;
        };

        struct VoxelDataKeyView {
            std::string_view algorithm;
            glm::uvec3 extents;
            uint32_t seed;
        };

        struct VoxelDataKeyHash {
            using is_transparent = void;

            std::size_t operator()(const VoxelDataKey &key) const {
                auto h = StringHash{}(key.algorithm);
                hash_combine(h, key.extents.x);
                hash_combine(h, key.extents.y);
                hash_combine(h, key.extents.z);
                hash_combine(h, key.seed);
                return h;
            }
            std::size_t operator()(const VoxelDataKeyView &key) const {
                auto h = StringHash{}(key.algorithm);
                hash_combine(h, key.extents.x);
                hash_combine(h, key.extents.y);
                hash_combine(h, key.extents.z);
                hash_combine(h, key.seed);
                return h;
            }
        };

        struct VoxelDataKeyEqual {
            using is_transparent = void;

            bool operator()(const VoxelDataKeyView &lhs, const VoxelDataKey &rhs) const {
                const std::string_view view = rhs.algorithm;
                return lhs.algorithm == view && lhs.extents == rhs.extents && lhs.seed == rhs.seed;
            }
            bool operator()(const VoxelDataKey &lhs, const VoxelDataKey &rhs) const {
                return lhs.algorithm == rhs.algorithm && lhs.extents == rhs.extents && lhs.seed == rhs.seed;
            }
        };

        PreservingMap<VoxelDataKey, Async<Mesh>, 10000, VoxelDataKeyHash, VoxelDataKeyEqual> activeVoxelData;
        std::vector<std::pair<AsyncPtr<Mesh>, AsyncPtr<Mesh>>> pendingMeshes; // pair<proxy, input>

        HeapVector<GPURenderableEntity> gpuRenderables;
        HeapVector<ecs::Entity> gpuRenderableEntities; // Indexes match 1-to-t with gpuRenderables
        HeapVector<ecs::Entity> entitiesPendingDelete; // Entities index into liveEntityState
        HeapVector<RenderableIndex> renderablesToFlush;
        DispatchQueue workQueue;
    };
} // namespace sp::vulkan
