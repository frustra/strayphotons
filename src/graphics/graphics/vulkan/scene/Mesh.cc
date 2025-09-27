/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Mesh.hh"

#include "assets/Gltf.hh"
#include "assets/GltfImpl.hh"
#include "common/Logging.hh"
#include "common/Tracing.hh"
#include "ecs/EcsImpl.hh"
#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/scene/VertexLayouts.hh"

namespace sp::vulkan {
    Mesh::Mesh(std::shared_ptr<const sp::Gltf> source, size_t meshIndex, GPUScene &scene, DeviceContext &device)
        : modelName(source->name), asset(source) {
        ZoneScoped;
        ZonePrintf("%s.%u", modelName, meshIndex);

        Assertf(meshIndex < source->meshes.size(), "Mesh index is out of range: %s.%u", modelName, meshIndex);
        auto &mesh = source->meshes[meshIndex];
        Assertf(mesh, "Mesh is undefined: %s.%u", modelName, meshIndex);
        for (auto &assetPrimitive : mesh->primitives) {
            indexCount += assetPrimitive.indexBuffer.Count();
            vertexCount += assetPrimitive.positionBuffer.Count();
            jointsCount += assetPrimitive.jointsBuffer.Count();
        }

        size_t totalBufferSize = sizeof(uint32) * indexCount + sizeof(SceneVertex) * vertexCount +
                                 sizeof(JointVertex) * jointsCount + sizeof(GPUMeshPrimitive) * primitives.size();
        size_t sample = device.frameBandwidthCounter.fetch_add(totalBufferSize);
        if (sample > CVarTransferBufferRateLimit.Get()) {
            auto delayFrames = sample / CVarTransferBufferRateLimit.Get();
            // Logf("Throttling Mesh %llu for %llu ms",
            //     totalBufferSize,
            //     (delayFrames * device.GetFrameInterval()).count() / 1000000);
            std::this_thread::sleep_for(delayFrames * device.GetFrameInterval());
        }

        indexBuffer = scene.indexBuffer->ArrayAllocate(indexCount);
        staging.indexBuffer = device.AllocateBuffer({sizeof(uint32), indexCount},
            vk::BufferUsageFlagBits::eTransferSrc,
            VMA_MEMORY_USAGE_CPU_ONLY);
        Assertf(indexBuffer->ByteSize() == staging.indexBuffer->ByteSize(), "index staging buffer size mismatch");

        auto indexData = (uint32 *)staging.indexBuffer->Mapped();
        auto indexDataStart = indexData;

        vertexBuffer = scene.vertexBuffer->ArrayAllocate(vertexCount);
        staging.vertexBuffer = device.AllocateBuffer({sizeof(SceneVertex), vertexCount},
            vk::BufferUsageFlagBits::eTransferSrc,
            VMA_MEMORY_USAGE_CPU_ONLY);
        Assertf(vertexBuffer->ByteSize() == staging.vertexBuffer->ByteSize(), "vertex staging buffer size mismatch");

        auto vertexData = (SceneVertex *)staging.vertexBuffer->Mapped();
        auto vertexDataStart = vertexData;

        JointVertex *jointsData = nullptr, *jointsDataStart = nullptr;

        if (jointsCount > 0) {
            jointsBuffer = scene.jointsBuffer->ArrayAllocate(jointsCount);
            staging.jointsBuffer = device.AllocateBuffer({sizeof(JointVertex), jointsCount},
                vk::BufferUsageFlagBits::eTransferSrc,
                VMA_MEMORY_USAGE_CPU_ONLY);
            Assertf(jointsBuffer->ByteSize() == staging.jointsBuffer->ByteSize(),
                "joints staging buffer size mismatch");

            jointsData = (JointVertex *)staging.jointsBuffer->Mapped();
            jointsDataStart = jointsData;
        }

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

            vkPrimitive.jointsVertexCount = assetPrimitive.jointsBuffer.Count();
            vkPrimitive.jointsVertexOffset = jointsData - jointsDataStart;

            vkPrimitive.center = glm::vec3(0);
            for (size_t i = 0; i < vkPrimitive.vertexCount; i++) {
                SceneVertex &vertex = *vertexData++;

                vertex.position = assetPrimitive.positionBuffer.Read(i);
                if (i < assetPrimitive.normalBuffer.Count()) vertex.normal = assetPrimitive.normalBuffer.Read(i);
                if (i < assetPrimitive.texcoordBuffer.Count()) vertex.uv = assetPrimitive.texcoordBuffer.Read(i);

                if (jointsData && i < assetPrimitive.jointsBuffer.Count()) {
                    Assert(i < assetPrimitive.weightsBuffer.Count(), "must have one weight per joint index");
                    JointVertex &joints = *jointsData++;
                    joints.jointIndexes = assetPrimitive.jointsBuffer.Read(i);
                    joints.jointWeights = assetPrimitive.weightsBuffer.Read(i);
                }

                vkPrimitive.center += vertex.position;
            }
            vkPrimitive.center /= vkPrimitive.vertexCount;

            vkPrimitive.baseColor = scene.textures.LoadGltfMaterial(source,
                assetPrimitive.materialIndex,
                TextureType::BaseColor);

            vkPrimitive.metallicRoughness = scene.textures.LoadGltfMaterial(source,
                assetPrimitive.materialIndex,
                TextureType::MetallicRoughness);
        }

        primitiveList = scene.primitiveLists->ArrayAllocate(primitives.size());
        staging.primitiveList = device.AllocateBuffer({sizeof(GPUMeshPrimitive), primitives.size()},
            vk::BufferUsageFlagBits::eTransferSrc,
            VMA_MEMORY_USAGE_CPU_ONLY);
        Assertf(primitiveList->ByteSize() == staging.primitiveList->ByteSize(),
            "primitive staging buffer size mismatch");

        modelEntry = scene.models->ArrayAllocate(1);
        staging.modelEntry = device.AllocateBuffer({sizeof(GPUMeshModel)},
            vk::BufferUsageFlagBits::eTransferSrc,
            VMA_MEMORY_USAGE_CPU_ONLY);
        Assertf(modelEntry->ByteSize() == staging.modelEntry->ByteSize(), "model staging buffer size mismatch");

        auto meshModel = (GPUMeshModel *)staging.modelEntry->Mapped();
        auto gpuPrimitives = (GPUMeshPrimitive *)staging.primitiveList->Mapped();
        {
            ZoneScopedN("CopyPrimitives");
            auto gpuPrim = gpuPrimitives;
            for (auto &p : primitives) {
                gpuPrim->indexCount = p.indexCount;
                gpuPrim->vertexCount = p.vertexCount;
                gpuPrim->firstIndex = p.indexOffset;
                gpuPrim->vertexOffset = p.vertexOffset;
                gpuPrim->jointsVertexOffset = p.jointsVertexCount > 0
                                                  ? jointsBuffer->ArrayOffset() + p.jointsVertexOffset
                                                  : 0xffffffff;
                gpuPrim->baseColorTexID = p.baseColor.index;
                gpuPrim->metallicRoughnessTexID = p.metallicRoughness.index;
                gpuPrim++;
            }
            meshModel->primitiveCount = primitives.size();
            meshModel->primitiveOffset = primitiveList->ArrayOffset();
            meshModel->indexOffset = indexBuffer->ArrayOffset();
            meshModel->vertexOffset = vertexBuffer->ArrayOffset();
        }

        InlineVector<DeviceContext::BufferTransfer, 5> transfer;
        transfer.emplace_back(staging.indexBuffer, indexBuffer);
        transfer.emplace_back(staging.vertexBuffer, vertexBuffer);
        if (staging.jointsBuffer) transfer.emplace_back(staging.jointsBuffer, jointsBuffer);
        transfer.emplace_back(staging.primitiveList, primitiveList);
        transfer.emplace_back(staging.modelEntry, modelEntry);

        // TODO replace with span constructor in vk-hpp v1.2.189
        staging.transferComplete = device.TransferBuffers({(uint32)transfer.size(), transfer.data()});
    }

    Mesh::~Mesh() {
        Tracef("Destroying Vulkan model %s", modelName);
    }

    uint32 Mesh::SceneIndex() const {
        return modelEntry->ArrayOffset();
    }
} // namespace sp::vulkan
