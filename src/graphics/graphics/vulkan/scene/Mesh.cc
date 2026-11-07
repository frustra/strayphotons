/*
 * Stray Photons - Copyright (C) 2026 Jacob Wirth & Justine Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Mesh.hh"

#include "assets/Asset.hh"
#include "assets/Gltf.hh"
#include "assets/GltfImpl.hh"
#include "common/Tracing.hh"
#include "ecs/EcsImpl.hh"
#include "glm/fwd.hpp"
#include "glm/gtx/string_cast.hpp"
#include "graphics/vulkan/core/Access.hh"
#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/core/Memory.hh"
#include "graphics/vulkan/core/Shader.hh"
#include "graphics/vulkan/core/VkCommon.hh"
#include "graphics/vulkan/scene/MarchingCubesData.h"
#include "graphics/vulkan/scene/VertexLayouts.hh"
#include "strayphotons/DispatchQueue.hh"
#include "strayphotons/InlineVector.hh"
#include "strayphotons/Logging.hh"
#include "strayphotons/Utility.hh"
#include "vulkan/vulkan.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <tracy/Tracy.hpp>

namespace sp::vulkan {
    Mesh::Mesh(std::shared_ptr<Gltf> source) : modelName(source ? source->name : ""), asset(source) {}

    Mesh::Mesh(std::shared_ptr<Gltf> source, size_t meshIndex, GPUScene &scene, DeviceContext &device)
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

        indexBuffer = scene.indexBuffer->ArrayAllocate(indexCount);
        staging.indexBuffer = device.AllocateBuffer({sizeof(uint32_t), indexCount},
            vk::BufferUsageFlagBits::eTransferSrc,
            VMA_MEMORY_USAGE_CPU_ONLY);
        Assertf(indexBuffer->ByteSize() == staging.indexBuffer->ByteSize(), "index staging buffer size mismatch");

        auto indexData = (uint32_t *)staging.indexBuffer->Mapped();
        auto indexDataStart = indexData;

        staging.indexBuffer->SetAccess(Access::None, Access::HostWrite);

        vertexBuffer = scene.vertexBuffer->ArrayAllocate(vertexCount);
        staging.vertexBuffer = device.AllocateBuffer({sizeof(SceneVertex), vertexCount},
            vk::BufferUsageFlagBits::eTransferSrc,
            VMA_MEMORY_USAGE_CPU_ONLY);
        Assertf(vertexBuffer->ByteSize() == staging.vertexBuffer->ByteSize(), "vertex staging buffer size mismatch");

        auto vertexData = (SceneVertex *)staging.vertexBuffer->Mapped();
        auto vertexDataStart = vertexData;

        staging.vertexBuffer->SetAccess(Access::None, Access::HostWrite);

        JointVertex *jointsData = nullptr, *jointsDataStart = nullptr;

        BufferPtr stagingJointsBuffer;
        if (jointsCount > 0) {
            jointsBuffer = scene.jointsBuffer->ArrayAllocate(jointsCount);
            stagingJointsBuffer = device.AllocateBuffer({sizeof(JointVertex), jointsCount},
                vk::BufferUsageFlagBits::eTransferSrc,
                VMA_MEMORY_USAGE_CPU_ONLY);
            Assertf(jointsBuffer->ByteSize() == stagingJointsBuffer->ByteSize(), "joints staging buffer size mismatch");

            jointsData = (JointVertex *)stagingJointsBuffer->Mapped();
            jointsDataStart = jointsData;

            stagingJointsBuffer->SetAccess(Access::None, Access::HostWrite);
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

        auto gpuPrimitives = (GPUMeshPrimitive *)staging.primitiveList->Mapped();
        auto meshModel = (GPUMeshModel *)staging.modelEntry->Mapped();
        staging.primitiveList->SetAccess(Access::None, Access::HostWrite);
        staging.modelEntry->SetAccess(Access::None, Access::HostWrite);
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

        std::shared_ptr<vk::UniqueFence> transferFence;
        {
            ZoneScopedN("TransferBuffers");
            CommandContextPtr cmd = device.GetFencedCommandContext(CommandContextType::TransferAsync);
            staging.indexBuffer->AcquireAccess(*cmd, Access::TransferRead);
            staging.vertexBuffer->AcquireAccess(*cmd, Access::TransferRead);
            staging.primitiveList->AcquireAccess(*cmd, Access::TransferRead);
            staging.modelEntry->AcquireAccess(*cmd, Access::TransferRead);
            if (stagingJointsBuffer) {
                stagingJointsBuffer->AcquireAccess(*cmd, Access::TransferRead);
                jointsBuffer->AcquireAccess(*cmd, Access::TransferWrite);
            }
            indexBuffer->AcquireAccess(*cmd, Access::TransferWrite);
            vertexBuffer->AcquireAccess(*cmd, Access::TransferWrite);
            primitiveList->AcquireAccess(*cmd, Access::TransferWrite);
            modelEntry->AcquireAccess(*cmd, Access::TransferWrite);

            vk::BufferCopy indexRegion{0, indexBuffer->ByteOffset(), staging.indexBuffer->ByteSize()};
            cmd->Raw().copyBuffer(*staging.indexBuffer, *indexBuffer, {indexRegion});
            vk::BufferCopy vertexRegion{0, vertexBuffer->ByteOffset(), staging.vertexBuffer->ByteSize()};
            cmd->Raw().copyBuffer(*staging.vertexBuffer, *vertexBuffer, {vertexRegion});
            if (stagingJointsBuffer) {
                vk::BufferCopy jointsRegion{0, jointsBuffer->ByteOffset(), stagingJointsBuffer->ByteSize()};
                cmd->Raw().copyBuffer(*stagingJointsBuffer, *jointsBuffer, {jointsRegion});
            }
            vk::BufferCopy primitiveRegion{0, primitiveList->ByteOffset(), staging.primitiveList->ByteSize()};
            cmd->Raw().copyBuffer(*staging.primitiveList, *primitiveList, {primitiveRegion});
            vk::BufferCopy modelRegion{0, modelEntry->ByteOffset(), staging.modelEntry->ByteSize()};
            cmd->Raw().copyBuffer(*staging.modelEntry, *modelEntry, {modelRegion});

            transferFence = cmd->Fence();
            device.Submit(cmd);
        }

        staging.transferComplete = device.ExecuteAfterFence(NewDispatchSource, transferFence, [] {});
    }

    Mesh::Mesh(std::string_view algorithm, glm::uvec3 extents, uint32_t seed, GPUScene &scene, DeviceContext &device)
        : modelName(std::string(algorithm) + "." + std::to_string(seed)) {
        ZoneScoped;
        ZonePrintf("%s [%s]", modelName, glm::to_string(extents));

        if (algorithm != "perlin_sphere") return;

        Assertf(extents != glm::uvec3(0), "Mesh extents is empty: %s", modelName);

        const auto &constants = LoadConstantBuffers(device);

        Logf("Loading mesh: %s [%s]", modelName, glm::to_string(extents));
        glm::uvec3 cubeGridSize = glm::max(glm::uvec3(2), extents) - 1u;
        size_t cubeCount = cubeGridSize.x * cubeGridSize.y * cubeGridSize.z;

        staging.vertexBuffer = device.AllocateBuffer({sizeof(SceneVertex), 4 * cubeCount},
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc,
            VMA_MEMORY_USAGE_GPU_ONLY);
        staging.indexBuffer = device.AllocateBuffer({sizeof(uint32_t), 1 + 5 * cubeCount},
            vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer |
                vk::BufferUsageFlagBits::eTransferSrc,
            VMA_MEMORY_USAGE_GPU_ONLY);
        staging.lookupBuffer = device.AllocateBuffer({sizeof(uint32_t), 2 + 4 * cubeCount},
            vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer |
                vk::BufferUsageFlagBits::eTransferSrc,
            VMA_MEMORY_USAGE_GPU_ONLY);
        staging.readbackBuffer = device.AllocateBuffer(
            {sizeof(uint32_t),
                2 + staging.indexBuffer->ArraySize() + staging.vertexBuffer->ByteSize() / sizeof(uint32_t)},
            vk::BufferUsageFlagBits::eTransferDst,
            VMA_MEMORY_USAGE_GPU_TO_CPU);
        static_assert(sizeof(SceneVertex) % sizeof(uint32_t) == 0, "SceneVertex size not multiple of uint32_t");

        auto &vkPrimitive = primitives.emplace_back();
        vkPrimitive.indexCount = 0; // Generated below
        vkPrimitive.indexOffset = 0;
        vkPrimitive.vertexCount = 0; // Generated below
        vkPrimitive.vertexOffset = 0;
        vkPrimitive.jointsVertexCount = 0;
        vkPrimitive.jointsVertexOffset = 0;
        vkPrimitive.center = glm::vec3(0);
        vkPrimitive.baseColor = scene.textures.GetSinglePixelIndex(glm::vec4(0.2, 0.2, 1.0, 1.0));
        vkPrimitive.metallicRoughness = scene.textures.GetSinglePixelIndex(glm::vec4(0.0, 1.0, 0.0, 1.0));

        primitiveList = scene.primitiveLists->ArrayAllocate(1);
        staging.primitiveList = device.AllocateBuffer({sizeof(GPUMeshPrimitive), 1},
            vk::BufferUsageFlagBits::eTransferSrc,
            VMA_MEMORY_USAGE_CPU_ONLY);
        Assertf(primitiveList->ByteSize() == staging.primitiveList->ByteSize(),
            "primitive staging buffer size mismatch");

        modelEntry = scene.models->ArrayAllocate(1);
        staging.modelEntry = device.AllocateBuffer({sizeof(GPUMeshModel)},
            vk::BufferUsageFlagBits::eTransferSrc,
            VMA_MEMORY_USAGE_CPU_ONLY);
        Assertf(modelEntry->ByteSize() == staging.modelEntry->ByteSize(), "model staging buffer size mismatch");

        {
            ZoneScopedN("InitStagingBuffers");
            CommandContextPtr cmd = device.GetFencedCommandContext(CommandContextType::ComputeAsync);
            staging.lookupBuffer->AcquireAccess(*cmd, Access::TransferWrite);
            staging.indexBuffer->AcquireAccess(*cmd, Access::TransferWrite);

            // Set seed
            cmd->Raw().fillBuffer(*staging.lookupBuffer, 0, sizeof(uint32_t), seed);
            // Set vertexCount to 0
            cmd->Raw().fillBuffer(*staging.lookupBuffer, sizeof(uint32_t), sizeof(uint32_t), 0u);
            // Set indexCount to 0
            cmd->Raw().fillBuffer(*staging.indexBuffer, 0, sizeof(uint32_t), 0u);

            device.Submit(cmd);
        }
        {
            ZoneScopedN("MarchChunkVertex");
            CommandContextPtr cmd = device.GetFencedCommandContext(CommandContextType::ComputeAsync);
            staging.vertexBuffer->AcquireAccess(*cmd, Access::ComputeShaderWrite);
            staging.lookupBuffer->AcquireAccess(*cmd, Access::ComputeShaderReadStorage);

            cmd->SetComputeShader("marching_cubes_vertex2.comp");
            cmd->SetShaderConstant(ShaderStage::Compute, "GRID_SIZE_X", extents.x);
            cmd->SetShaderConstant(ShaderStage::Compute, "GRID_SIZE_Y", extents.y);
            cmd->SetShaderConstant(ShaderStage::Compute, "GRID_SIZE_Z", extents.z);

            cmd->SetStorageBuffer("IndexLookupBuffer", staging.lookupBuffer);
            cmd->SetStorageBuffer("VertexBuffer", staging.vertexBuffer);

            glm::uvec3 groups = (cubeGridSize + 7u) / 8u;
            cmd->Dispatch(groups.x, groups.y, groups.z);

            staging.lookupBuffer->SetAccess(Access::ComputeShaderReadStorage, Access::ComputeShaderWrite);

            device.Submit(cmd);
        }
        {
            ZoneScopedN("MarchChunkTriangle");
            CommandContextPtr cmd = device.GetFencedCommandContext(CommandContextType::ComputeAsync);
            constants.triangleIndexBuffer->AcquireAccess(*cmd, Access::ComputeShaderReadStorage);
            constants.caseInteriorEdges->AcquireAccess(*cmd, Access::ComputeShaderReadStorage);
            constants.triangleCaseOffsets->AcquireAccess(*cmd, Access::ComputeShaderReadStorage);
            constants.caseTests->AcquireAccess(*cmd, Access::ComputeShaderReadStorage);

            staging.lookupBuffer->AcquireAccess(*cmd, Access::ComputeShaderReadStorage);
            staging.indexBuffer->AcquireAccess(*cmd, Access::ComputeShaderReadStorage);

            cmd->SetComputeShader("marching_cubes_triangle2.comp");
            cmd->SetShaderConstant(ShaderStage::Compute, "GRID_SIZE_X", extents.x);
            cmd->SetShaderConstant(ShaderStage::Compute, "GRID_SIZE_Y", extents.y);
            cmd->SetShaderConstant(ShaderStage::Compute, "GRID_SIZE_Z", extents.z);

            cmd->SetStorageBuffer("TriangleIndexBuffer", constants.triangleIndexBuffer);
            cmd->SetStorageBuffer("CaseInteriorEdges", constants.caseInteriorEdges);
            cmd->SetStorageBuffer("TriangleCaseOffsets", constants.triangleCaseOffsets);
            cmd->SetStorageBuffer("CaseTests", constants.caseTests);

            cmd->SetStorageBuffer("IndexLookupBuffer", staging.lookupBuffer);
            cmd->SetStorageBuffer("IndexBuffer", staging.indexBuffer);

            glm::uvec3 groups = (cubeGridSize + 7u) / 8u;
            cmd->Dispatch(groups.x, groups.y, groups.z);

            staging.indexBuffer->SetAccess(Access::ComputeShaderReadStorage, Access::ComputeShaderWrite);

            device.Submit(cmd);
        }
        std::shared_ptr<vk::UniqueFence> readbackFence;
        {
            ZoneScopedN("BufferReadback");
            CommandContextPtr cmd = device.GetFencedCommandContext(CommandContextType::ComputeAsync);
            staging.vertexBuffer->AcquireAccess(*cmd, Access::TransferRead);
            staging.lookupBuffer->AcquireAccess(*cmd, Access::TransferRead);
            staging.indexBuffer->AcquireAccess(*cmd, Access::TransferRead);
            staging.readbackBuffer->AcquireAccess(*cmd, Access::TransferWrite);

            vk::BufferCopy vertexCountRegion(sizeof(uint32_t), 0, sizeof(uint32_t));
            vk::BufferCopy indexCountRegion(0, sizeof(uint32_t), sizeof(uint32_t));
            vk::BufferCopy vertexDataRegion(0, sizeof(uint32_t) * 2, staging.vertexBuffer->ByteSize());
            vk::BufferCopy indexDataRegion(sizeof(uint32_t),
                vertexDataRegion.dstOffset + vertexDataRegion.size,
                staging.indexBuffer->ByteSize() - sizeof(uint32_t));
            cmd->Raw().copyBuffer(*staging.lookupBuffer, *staging.readbackBuffer, {vertexCountRegion});
            cmd->Raw().copyBuffer(*staging.indexBuffer, *staging.readbackBuffer, {indexCountRegion, indexDataRegion});
            cmd->Raw().copyBuffer(*staging.vertexBuffer, *staging.readbackBuffer, {vertexDataRegion});

            cmd->BufferBarrier(staging.readbackBuffer, vk::PipelineStageFlagBits::eHost, vk::AccessFlagBits::eHostRead);

            readbackFence = cmd->Fence();
            device.Submit(cmd);
        }
        asset = std::make_shared<Gltf>(modelName);
        staging.transferComplete = device.ExecuteAfterFence(NewDispatchSource, readbackFence, [this, &scene, &device] {
            const uint8_t *readbackData = (const uint8_t *)staging.readbackBuffer->Mapped();
            vertexCount = *reinterpret_cast<const uint32_t *>(readbackData);
            indexCount = *reinterpret_cast<const uint32_t *>(readbackData + sizeof(uint32_t));
            Logf("Vertex count: %u, Index count: %u, Model name: %s", vertexCount, indexCount, modelName);
            if (vertexCount == 0 || indexCount == 0) return;

            std::vector<uint8_t> buf(readbackData, readbackData + staging.readbackBuffer->ByteSize());
            asset->asset = std::make_shared<Asset>(std::move(buf));
            gltf::Mesh generatedMesh({
                gltf::Mesh::Primitive(gltf::Mesh::DrawMode::Triangles,
                    gltf::Accessor<uint32_t, uint16_t, uint8_t>(typeid(uint32_t),
                        indexCount,
                        asset->asset->Buffer(),
                        sizeof(uint32_t),
                        sizeof(uint32_t) * 2 + staging.vertexBuffer->ByteSize()),
                    gltf::Accessor<glm::vec3>(typeid(glm::vec3),
                        vertexCount,
                        asset->asset->Buffer(),
                        sizeof(SceneVertex),
                        sizeof(uint32_t) * 2)),
            });
            asset->meshes.emplace_back(generatedMesh);

            indexBuffer = scene.indexBuffer->ArrayAllocate(indexCount);
            Assertf(indexBuffer->ByteSize() <= staging.indexBuffer->ByteSize(), "index staging buffer size overflow");

            vertexBuffer = scene.vertexBuffer->ArrayAllocate(vertexCount);
            Assertf(vertexBuffer->ByteSize() <= staging.vertexBuffer->ByteSize(),
                "vertex staging buffer size overflow");

            auto &vkPrimitive = primitives[0];
            vkPrimitive.indexCount = indexCount;
            vkPrimitive.vertexCount = vertexCount;

            {
                ZoneScopedN("WritePrimitive");
                GPUMeshPrimitive *gpuPrimitive;
                staging.primitiveList->SetAccess(Access::None, Access::HostWrite);
                staging.primitiveList->Map((void **)&gpuPrimitive);
                gpuPrimitive->indexCount = vkPrimitive.indexCount;
                gpuPrimitive->vertexCount = vkPrimitive.vertexCount;
                gpuPrimitive->firstIndex = vkPrimitive.indexOffset;
                gpuPrimitive->vertexOffset = vkPrimitive.vertexOffset;
                gpuPrimitive->jointsVertexOffset = 0xFFFFFFFF;
                gpuPrimitive->baseColorTexID = vkPrimitive.baseColor.index;
                gpuPrimitive->metallicRoughnessTexID = vkPrimitive.metallicRoughness.index;
                staging.primitiveList->Unmap();
                staging.primitiveList->Flush();

                GPUMeshModel *meshModel;
                staging.modelEntry->SetAccess(Access::None, Access::HostWrite);
                staging.modelEntry->Map((void **)&meshModel);
                meshModel->primitiveCount = 1;
                meshModel->primitiveOffset = primitiveList->ArrayOffset();
                meshModel->indexOffset = indexBuffer->ArrayOffset();
                meshModel->vertexOffset = vertexBuffer->ArrayOffset();
                staging.modelEntry->Unmap();
                staging.modelEntry->Flush();
            }

            {
                ZoneScopedN("TransferBuffers");
                CommandContextPtr cmd = device.GetFencedCommandContext(CommandContextType::ComputeAsync);
                staging.vertexBuffer->AcquireAccess(*cmd, Access::TransferRead);
                staging.indexBuffer->AcquireAccess(*cmd, Access::TransferRead);
                staging.primitiveList->AcquireAccess(*cmd, Access::TransferRead);
                staging.modelEntry->AcquireAccess(*cmd, Access::TransferRead);
                vertexBuffer->AcquireAccess(*cmd, Access::TransferWrite);
                indexBuffer->AcquireAccess(*cmd, Access::TransferWrite);
                primitiveList->AcquireAccess(*cmd, Access::TransferWrite);
                modelEntry->AcquireAccess(*cmd, Access::TransferWrite);

                vk::BufferCopy vertexRegion(0, vertexBuffer->ByteOffset(), sizeof(SceneVertex) * vertexCount);
                vk::BufferCopy indexRegion(sizeof(uint32_t), indexBuffer->ByteOffset(), sizeof(uint32_t) * indexCount);
                vk::BufferCopy primitiveRegion(0, primitiveList->ByteOffset(), sizeof(GPUMeshPrimitive));
                vk::BufferCopy modelRegion(0, modelEntry->ByteOffset(), sizeof(GPUMeshModel));
                cmd->Raw().copyBuffer(*staging.vertexBuffer, *vertexBuffer, {vertexRegion});
                cmd->Raw().copyBuffer(*staging.indexBuffer, *indexBuffer, {indexRegion});
                cmd->Raw().copyBuffer(*staging.primitiveList, *primitiveList, {primitiveRegion});
                cmd->Raw().copyBuffer(*staging.modelEntry, *modelEntry, {modelRegion});

                device.Submit(cmd);

                Logf("Buffer transfer submitted %s", modelName);
                Assertf(!staging.transferComplete->Ready(), "Transfer complete too early: %s", modelName);
            }
        });
    }

    Mesh::~Mesh() {
        Tracef("Destroying Vulkan model %s", modelName);
    }

    uint32_t Mesh::SceneIndex() const {
        return modelEntry->ArrayOffset();
    }

    Mesh::ConstantBuffers &Mesh::GetConstantBuffers() {
        static Mesh::ConstantBuffers constants;
        return constants;
    }

    const Mesh::ConstantBuffers &Mesh::LoadConstantBuffers(DeviceContext &device) {
        ConstantBuffers &constants = GetConstantBuffers();
        if (!constants.triangleIndexBuffer) {
            ZoneScopedN("TriangleIndexBuffer");
            constants.triangleIndexBuffer = device.AllocateBuffer(
                {sizeof(triangleIndexBuffer[0]), sizeof(triangleIndexBuffer) / sizeof(triangleIndexBuffer[0])},
                vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
                VMA_MEMORY_USAGE_CPU_TO_GPU);
            constants.triangleIndexBuffer->SetAccess(Access::None, Access::HostWrite);
            constants.triangleIndexBuffer->CopyFrom(&triangleIndexBuffer[0],
                constants.triangleIndexBuffer->ArraySize());
        }
        if (!constants.caseInteriorEdges) {
            ZoneScopedN("CaseInteriorEdges");
            constants.caseInteriorEdges = device.AllocateBuffer(
                {sizeof(caseInteriorEdge[0][0]), sizeof(caseInteriorEdge) / sizeof(caseInteriorEdge[0][0])},
                vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
                VMA_MEMORY_USAGE_CPU_TO_GPU);
            constants.caseInteriorEdges->SetAccess(Access::None, Access::HostWrite);
            constants.caseInteriorEdges->CopyFrom(&caseInteriorEdge[0][0], constants.caseInteriorEdges->ArraySize());
        }
        if (!constants.triangleCaseOffsets) {
            ZoneScopedN("TriangleCaseOffsets");
            constants.triangleCaseOffsets = device.AllocateBuffer(
                {sizeof(triangleCaseOffset[0][0][0]), sizeof(triangleCaseOffset) / sizeof(triangleCaseOffset[0][0][0])},
                vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
                VMA_MEMORY_USAGE_CPU_TO_GPU);
            constants.triangleCaseOffsets->SetAccess(Access::None, Access::HostWrite);
            constants.triangleCaseOffsets->CopyFrom(&triangleCaseOffset[0][0][0],
                constants.triangleCaseOffsets->ArraySize());
        }
        if (!constants.caseTests) {
            ZoneScopedN("CaseTests");
            constants.caseTests = device.AllocateBuffer(
                {sizeof(caseTests[0][0]), sizeof(caseTests) / sizeof(caseTests[0][0])},
                vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
                VMA_MEMORY_USAGE_CPU_TO_GPU);
            constants.caseTests->SetAccess(Access::None, Access::HostWrite);
            constants.caseTests->CopyFrom(&caseTests[0][0], constants.caseTests->ArraySize());
        }
        return constants;
    }

    void Mesh::UnloadConstantBuffers() {
        ConstantBuffers &constants = GetConstantBuffers();
        constants = {};
    }
} // namespace sp::vulkan
