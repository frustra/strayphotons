#include "Mesh.hh"

#include "assets/Gltf.hh"
#include "assets/GltfImpl.hh"
#include "core/Logging.hh"
#include "core/Tracing.hh"
#include "ecs/EcsImpl.hh"
#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/scene/VertexLayouts.hh"

#include <tiny_gltf.h>

namespace sp::vulkan {
    Mesh::Mesh(std::shared_ptr<const sp::Gltf> source, size_t meshIndex, GPUScene &scene, DeviceContext &device)
        : modelName(source->name), asset(source) {
        ZoneScoped;
        ZonePrintf("%s.%u", modelName, meshIndex);
        // TODO: cache the output somewhere. Keeping the conversion code in
        // the engine will be useful for any dynamic loading in the future,
        // but we don't want to do it every time a model is loaded.

        Assertf(meshIndex < source->meshes.size(), "Mesh index is out of range: %s.%u", modelName, meshIndex);
        auto &mesh = source->meshes[meshIndex];
        Assertf(mesh, "Mesh is undefined: %s.%u", modelName, meshIndex);
        for (auto &assetPrimitive : mesh->primitives) {
            indexCount += assetPrimitive.indexBuffer.Count();
            vertexCount += assetPrimitive.positionBuffer.Count();
        }

        indexBuffer = scene.indexBuffer->ArrayAllocate(indexCount, sizeof(uint32));
        auto indexData = (uint32 *)indexBuffer->Mapped();
        auto indexDataStart = indexData;

        vertexBuffer = scene.vertexBuffer->ArrayAllocate(vertexCount, sizeof(SceneVertex));
        auto vertexData = (SceneVertex *)vertexBuffer->Mapped();
        auto vertexDataStart = vertexData;

        for (auto &assetPrimitive : mesh->primitives) {
            ZoneScopedN("CreatePrimitive");
            // TODO: this implementation assumes a lot about the model format,
            // and asserts the assumptions. It would be better to support more
            // kinds of inputs, and convert the data rather than just failing.
            Assert(assetPrimitive.drawMode == sp::gltf::Mesh::DrawMode::Triangles, "draw mode must be Triangles");

            auto &vkPrimitive = primitives.emplace_back();

            vkPrimitive.indexCount = assetPrimitive.indexBuffer.Count();
            vkPrimitive.indexOffset = indexData - indexDataStart;

            for (size_t i = 0; i < vkPrimitive.indexCount; i++) {
                *indexData++ = assetPrimitive.indexBuffer.Read(i);
            }

            vkPrimitive.vertexCount = assetPrimitive.positionBuffer.Count();
            vkPrimitive.vertexOffset = vertexData - vertexDataStart;

            for (size_t i = 0; i < vkPrimitive.vertexCount; i++) {
                SceneVertex &vertex = *vertexData++;

                vertex.position = assetPrimitive.positionBuffer.Read(i);
                if (i < assetPrimitive.normalBuffer.Count()) vertex.normal = assetPrimitive.normalBuffer.Read(i);
                if (i < assetPrimitive.texcoordBuffer.Count()) vertex.uv = assetPrimitive.texcoordBuffer.Read(i);
            }

            vkPrimitive.baseColor = scene.textures.LoadGltfMaterial(source,
                assetPrimitive.materialIndex,
                TextureType::BaseColor);

            vkPrimitive.metallicRoughness = scene.textures.LoadGltfMaterial(source,
                assetPrimitive.materialIndex,
                TextureType::MetallicRoughness);
        }

        primitiveList = scene.primitiveLists->ArrayAllocate(primitives.size(), sizeof(GPUMeshPrimitive));
        modelEntry = scene.models->ArrayAllocate(1, sizeof(GPUMeshModel));

        auto meshModel = (GPUMeshModel *)modelEntry->Mapped();
        auto gpuPrimitives = (GPUMeshPrimitive *)primitiveList->Mapped();
        {
            ZoneScopedN("CopyPrimitives");
            auto gpuPrim = gpuPrimitives;
            for (auto &p : primitives) {
                gpuPrim->indexCount = p.indexCount;
                gpuPrim->vertexCount = p.vertexCount;
                gpuPrim->firstIndex = p.indexOffset;
                gpuPrim->vertexOffset = p.vertexOffset;
                gpuPrim->baseColorTexID = p.baseColor.index;
                gpuPrim->metallicRoughnessTexID = p.metallicRoughness.index;
                gpuPrim++;
            }
            meshModel->primitiveCount = primitives.size();
            meshModel->primitiveOffset = primitiveList->ArrayOffset();
            meshModel->indexOffset = indexBuffer->ArrayOffset();
            meshModel->vertexOffset = vertexBuffer->ArrayOffset();
        }
    }

    Mesh::~Mesh() {
        Tracef("Destroying Vulkan model %s", modelName);
    }

    uint32 Mesh::SceneIndex() const {
        return modelEntry->ArrayOffset();
    }
} // namespace sp::vulkan
