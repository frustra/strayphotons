/*
 * Stray Photons - Copyright (C) 2026 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "MarchingCubes.hh"

#include "MarchingCubesData.h"
#include "ecs/EcsImpl.hh"
#include "ecs/components/Renderable.hh"
#include "graphics/vulkan/core/Access.hh"
#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/core/Memory.hh"
#include "graphics/vulkan/core/Shader.hh"
#include "graphics/vulkan/core/VkCommon.hh"
#include "graphics/vulkan/render_graph/PassBuilder.hh"
#include "graphics/vulkan/render_passes/Readback.hh"
#include "graphics/vulkan/render_passes/Voxels.hh"
#include "graphics/vulkan/scene/VertexLayouts.hh"

#include <cstdint>
#include <string>

namespace sp::vulkan::renderer {
    CVar<uint32_t> CVarEnableMarchingCubes("r.EnableMarchingCubes",
        0,
        "Enable converting the voxel grid into a triangle mesh (0: off, 1: basic, 2: advanced)");
    CVar<uint32_t> CVarMarchingCubesLayer("r.MarchingCubesLayer",
        0,
        "The voxel grid layer to convert to a triangle mesh");

    MarchingCubes::MarchingCubes(GPUScene &scene) { // : scene(scene) {
        funcs.Register("printmarchingcubes", "Print graphics debug information", [this]() {
            if (debugThisFrame.test_and_set()) {
                Warnf("Graphics frame already flagged for debug printing");
            }
        });
    }

    void MarchingCubes::AddMarchingCubes(RenderGraph &graph, const Voxels &voxels) {
        uint32_t version = CVarEnableMarchingCubes.Get();
        uint32_t voxelLayerCount = voxels.GetLayerCount();
        glm::ivec3 voxelGridSize = voxels.GetGridSize();
        if (voxelGridSize == glm::ivec3(0) || voxelLayerCount == 0 || !CVarEnableVoxels.Get() ||
            !CVarEnableVoxels2.Get() || version == 0) {
            return;
        }
        uint32_t cubeLayer = CVarMarchingCubesLayer.Get();
        cubeLayer = std::min(cubeLayer, voxelLayerCount - 1);
        glm::uvec3 cubeGridSize = glm::max(glm::ivec3(1), voxelGridSize - 1);
        cubeGridSize >>= cubeLayer;
        size_t cubeCount = cubeGridSize.x * cubeGridSize.y * cubeGridSize.z;

        ZoneScoped;
        auto scope = graph.Scope("MarchingCubes");

        graph.AddPass("Init")
            .Build([&](rg::PassBuilder &builder) {
                builder.CreateBuffer("VertexBuffer",
                    {sizeof(SceneVertex), 4 * cubeCount},
                    Residency::GPU_ONLY,
                    Access::None);

                builder.CreateBuffer("IndexLookup",
                    {sizeof(uint32_t), 2 + 4 * cubeCount},
                    Residency::GPU_ONLY,
                    Access::TransferWrite);

                builder.CreateBuffer("IndexBuffer",
                    {sizeof(uint32_t), (sizeof(VkDrawIndexedIndirectCommand) / sizeof(uint32_t)) + 5 * cubeCount},
                    Residency::GPU_ONLY,
                    Access::TransferWrite);
            })
            .Execute([](rg::Resources &resources, CommandContext &cmd) {
                static uint32_t time = 0;
                auto lookupBuffer = resources.GetBuffer("IndexLookup");
                cmd.Raw().fillBuffer(*lookupBuffer, 0, sizeof(uint32_t), time++);
                cmd.Raw().fillBuffer(*lookupBuffer, sizeof(uint32_t), sizeof(uint32_t), 1u);
                cmd.Raw().fillBuffer(*lookupBuffer, 2 * sizeof(uint32_t), sizeof(glm::vec3), 0u);
                auto indexBuffer = resources.GetBuffer("IndexBuffer");
                cmd.Raw().fillBuffer(*indexBuffer, 0, sizeof(VkDrawIndexedIndirectCommand), 0u);
                cmd.Raw().fillBuffer(*indexBuffer,
                    offsetof(VkDrawIndexedIndirectCommand, instanceCount),
                    sizeof(uint32_t),
                    1u);
            });
        graph.AddPass("UploadTriangleData")
            .Build([&](rg::PassBuilder &builder) {
                builder.CreateBuffer("TriangleIndexBuffer",
                    {sizeof(triangleIndexBuffer[0]), sizeof(triangleIndexBuffer) / sizeof(triangleIndexBuffer[0])},
                    Residency::CPU_TO_GPU,
                    Access::HostWrite);
                builder.CreateBuffer("CaseInteriorEdges",
                    {sizeof(caseInteriorEdge[0][0]), sizeof(caseInteriorEdge) / sizeof(caseInteriorEdge[0][0])},
                    Residency::CPU_TO_GPU,
                    Access::HostWrite);
                builder.CreateBuffer("TriangleCaseOffsets",
                    {sizeof(triangleCaseOffset[0][0][0]),
                        sizeof(triangleCaseOffset) / sizeof(triangleCaseOffset[0][0][0])},
                    Residency::CPU_TO_GPU,
                    Access::HostWrite);
                builder.CreateBuffer("CaseTests",
                    {sizeof(caseTests[0][0]), sizeof(caseTests) / sizeof(caseTests[0][0])},
                    Residency::CPU_TO_GPU,
                    Access::HostWrite);
            })
            .Execute([](rg::Resources &resources, DeviceContext &device) {
                resources.GetBuffer("TriangleIndexBuffer")
                    ->CopyFrom(&triangleIndexBuffer[0], sizeof(triangleIndexBuffer) / sizeof(triangleIndexBuffer[0]));
                resources.GetBuffer("CaseInteriorEdges")
                    ->CopyFrom(&caseInteriorEdge[0][0], sizeof(caseInteriorEdge) / sizeof(caseInteriorEdge[0][0]));
                resources.GetBuffer("TriangleCaseOffsets")
                    ->CopyFrom(&triangleCaseOffset[0][0][0],
                        sizeof(triangleCaseOffset) / sizeof(triangleCaseOffset[0][0][0]));
                resources.GetBuffer("CaseTests")
                    ->CopyFrom(&caseTests[0][0], sizeof(caseTests) / sizeof(caseTests[0][0]));
            });

        graph.AddPass("MarchChunkVertex")
            .Build([&](rg::PassBuilder &builder) {
                builder.Read("Voxels/Radiance", Access::ComputeShaderSampleImage);

                builder.Read("IndexLookup", Access::ComputeShaderReadStorage);
                builder.Write("IndexLookup", Access::ComputeShaderWrite);
                builder.Write("VertexBuffer", Access::ComputeShaderWrite);
                builder.ReadUniform("VoxelState");
            })

            .Execute([cubeLayer, cubeGridSize, version](rg::Resources &resources, CommandContext &cmd) {
                if (version == 2) {
                    cmd.SetComputeShader("marching_cubes_vertex2.comp");
                } else {
                    cmd.SetComputeShader("marching_cubes_vertex.comp");
                }
                cmd.SetShaderConstant(ShaderStage::Compute, "CUBE_LAYER", cubeLayer);

                cmd.SetUniformBuffer("VoxelStateUniform", "VoxelState");

                cmd.SetImageView("voxelRadiance", resources.GetImageMipView("Voxels/Radiance", cubeLayer));
                cmd.SetStorageBuffer("IndexLookupBuffer", "IndexLookup");
                cmd.SetStorageBuffer("VertexBuffer", "VertexBuffer");

                glm::uvec3 groups = (cubeGridSize + 7u) / 8u;
                cmd.Dispatch(groups.x, groups.y, groups.z);
            });

        graph.AddPass("MarchChunkTriangle")
            .Build([&](rg::PassBuilder &builder) {
                builder.Read("Voxels/Radiance", Access::ComputeShaderSampleImage);

                builder.Read("IndexLookup", Access::ComputeShaderReadStorage);
                builder.Read("IndexBuffer", Access::ComputeShaderReadStorage);
                builder.Write("IndexBuffer", Access::ComputeShaderWrite);
                builder.Read("TriangleIndexBuffer", Access::ComputeShaderReadStorage);
                builder.Read("CaseInteriorEdges", Access::ComputeShaderReadStorage);
                builder.Read("TriangleCaseOffsets", Access::ComputeShaderReadStorage);
                builder.Read("CaseTests", Access::ComputeShaderReadStorage);
                builder.ReadUniform("VoxelState");
            })

            .Execute([cubeLayer, cubeGridSize, version](rg::Resources &resources, CommandContext &cmd) {
                if (version == 2) {
                    cmd.SetComputeShader("marching_cubes_triangle2.comp");

                    cmd.SetStorageBuffer("TriangleIndexBuffer", "TriangleIndexBuffer");
                    cmd.SetStorageBuffer("CaseInteriorEdges", "CaseInteriorEdges");
                    cmd.SetStorageBuffer("TriangleCaseOffsets", "TriangleCaseOffsets");
                    cmd.SetStorageBuffer("CaseTests", "CaseTests");
                } else {
                    cmd.SetComputeShader("marching_cubes_triangle.comp");
                }
                cmd.SetShaderConstant(ShaderStage::Compute, "CUBE_LAYER", cubeLayer);

                cmd.SetUniformBuffer("VoxelStateUniform", "VoxelState");

                cmd.SetImageView("voxelRadiance", resources.GetImageMipView("Voxels/Radiance", cubeLayer));
                cmd.SetStorageBuffer("IndexLookupBuffer", "IndexLookup");
                cmd.SetStorageBuffer("IndexBuffer", "IndexBuffer");

                glm::uvec3 groups = (cubeGridSize + 7u) / 8u;
                cmd.Dispatch(groups.x, groups.y, groups.z);
            });

        bool printDebug = debugThisFrame.test();
        debugThisFrame.clear();
        if (printDebug) {
            AddBufferReadback(graph, "IndexBuffer", 0, sizeof(uint32_t) * (1 + 3 * cubeCount), [](BufferPtr buffer) {
                ZoneScopedN("IndexBufferReadback");
                const uint32_t *indexData = (const uint32_t *)buffer->Mapped();
                uint32_t triangleCount = indexData[0];
                Logf("Triangle count: %u", triangleCount);
                // for (uint32_t triangleIndex = 0; triangleIndex < triangleCount; triangleIndex++) {
                //     Logf("Triangle %u = [%u %u %u]",
                //         triangleIndex,
                //         indexData[triangleIndex * 3],
                //         indexData[triangleIndex * 3 + 1],
                //         indexData[triangleIndex * 3 + 2]);
                // }
            });
        }
    }
} // namespace sp::vulkan::renderer
