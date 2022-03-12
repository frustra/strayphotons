#pragma once

#include "assets/Async.hh"
#include "assets/Gltf.hh"
#include "core/DispatchQueue.hh"
#include "core/PreservingMap.hh"
#include "graphics/vulkan/core/Common.hh"
#include "graphics/vulkan/core/Image.hh"
#include "graphics/vulkan/core/Memory.hh"
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
        uint32 meshIndex;
        uint32 visibilityMask;
        uint32 vertexOffset;
        float _padding[1];
    };
    static_assert(sizeof(GPURenderableEntity) % sizeof(glm::vec4) == 0, "std430 alignment");

    class GPUScene {
    private:
        DeviceContext &device;
        DispatchQueue workQueue;

    public:
        GPUScene(DeviceContext &device);
        void Flush();
        void LoadState(rg::RenderGraph &graph, ecs::Lock<ecs::Read<ecs::Renderable, ecs::TransformSnapshot>> lock);
        shared_ptr<Mesh> LoadMesh(const std::shared_ptr<const sp::Gltf> &model, size_t meshIndex);

        struct DrawBufferIDs {
            rg::ResourceID drawCommandsBuffer; // first 4 bytes are the number of draws
            rg::ResourceID drawParamsBuffer;
        };

        DrawBufferIDs GenerateDrawsForView(rg::RenderGraph &graph,
            ecs::Renderable::VisibilityMask viewMask,
            uint32 instanceCount = 1);

        void DrawSceneIndirect(CommandContext &cmd,
            BufferPtr vertexBuffer,
            BufferPtr drawCommandsBuffer,
            BufferPtr drawParamsBuffer);

        void AddGeometryWarp(rg::RenderGraph &graph);

        BufferPtr indexBuffer;
        BufferPtr vertexBuffer;
        BufferPtr primitiveLists;
        BufferPtr models;

        uint32 renderableCount = 0;

        uint32 vertexCount = 0;
        uint32 primitiveCount = 0;
        uint32 primitiveCountPowerOfTwo = 1; // Always at least 1. Used to size draw command buffers.

        TextureSet textures;

    private:
        void FlushMeshes();
        PreservingMap<string, Mesh> activeMeshes;
        vector<std::pair<std::shared_ptr<const sp::Gltf>, size_t>> meshesToLoad;
        vector<GPURenderableEntity> renderables;
    };
} // namespace sp::vulkan
