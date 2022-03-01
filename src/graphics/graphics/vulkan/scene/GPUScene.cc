#include "GPUScene.hh"

#include "assets/GltfImpl.hh"
#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/scene/Mesh.hh"
#include "graphics/vulkan/scene/VertexLayouts.hh"

namespace sp::vulkan {
    GPUScene::GPUScene(DeviceContext &device) : device(device), workQueue("", 0), textures(device, workQueue) {
        indexBuffer = device.AllocateBuffer(1024 * 1024 * 10,
            vk::BufferUsageFlagBits::eIndexBuffer,
            VMA_MEMORY_USAGE_CPU_TO_GPU);

        vertexBuffer = device.AllocateBuffer(1024 * 1024 * 100,
            vk::BufferUsageFlagBits::eVertexBuffer,
            VMA_MEMORY_USAGE_CPU_TO_GPU);

        primitiveLists = device.AllocateBuffer(1024 * 1024,
            vk::BufferUsageFlagBits::eStorageBuffer,
            VMA_MEMORY_USAGE_CPU_TO_GPU);

        models = device.AllocateBuffer(1024 * 10, vk::BufferUsageFlagBits::eStorageBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU);
    }

    void GPUScene::Flush() {
        workQueue.Flush();
        textures.Flush();
        FlushMeshes();
    }

    void GPUScene::LoadState(rg::RenderGraph &graph,
        ecs::Lock<ecs::Read<ecs::Renderable, ecs::TransformSnapshot>> lock) {
        renderables.clear();
        renderableCount = 0;
        primitiveCount = 0;
        vertexCount = 0;

        for (auto &ent : lock.EntitiesWith<ecs::Renderable>()) {
            if (!ent.Has<ecs::TransformSnapshot>(lock)) continue;

            auto &renderable = ent.Get<ecs::Renderable>(lock);
            if (!renderable.model || !renderable.model->Ready()) continue;

            auto model = renderable.model->Get();
            if (!model) continue;

            auto meshName = model->name + "." + std::to_string(renderable.meshIndex);
            auto vkMesh = activeMeshes.Load(meshName);
            if (!vkMesh) {
                meshesToLoad.emplace_back(model, renderable.meshIndex);
                continue;
            }
            if (!vkMesh->CheckReady()) continue;

            GPURenderableEntity gpuRenderable;
            gpuRenderable.modelToWorld = ent.Get<ecs::TransformSnapshot>(lock).matrix;
            gpuRenderable.visibilityMask = renderable.visibility.to_ulong();
            gpuRenderable.meshIndex = vkMesh->SceneIndex();
            gpuRenderable.vertexOffset = vertexCount;
            renderables.push_back(gpuRenderable);
            renderableCount++;
            primitiveCount += vkMesh->PrimitiveCount();
            vertexCount += vkMesh->VertexCount();
        }

        primitiveCountPowerOfTwo = std::max(1u, CeilToPowerOfTwo(primitiveCount));

        graph.AddPass("SceneState")
            .Build([&](rg::PassBuilder &builder) {
                builder.BufferCreate("RenderableEntities",
                    renderables.size() * sizeof(renderables.front()),
                    rg::BufferUsage::eTransferDst,
                    Residency::CPU_TO_GPU);
            })
            .Execute([this](rg::Resources &resources, DeviceContext &device) {
                resources.GetBuffer("RenderableEntities")->CopyFrom(renderables.data(), renderables.size());
            });
    }

    shared_ptr<Mesh> GPUScene::LoadMesh(const std::shared_ptr<const sp::Gltf> &model, size_t meshIndex) {
        auto meshName = model->name + "." + std::to_string(meshIndex);
        auto vkMesh = activeMeshes.Load(meshName);
        if (!vkMesh) { meshesToLoad.emplace_back(model, meshIndex); }
        return vkMesh;
    }

    void GPUScene::FlushMeshes() {
        activeMeshes.Tick(std::chrono::milliseconds(33));

        for (int i = (int)meshesToLoad.size() - 1; i >= 0; i--) {
            auto &[model, meshIndex] = meshesToLoad[i];
            auto meshName = model->name + "." + std::to_string(meshIndex);
            if (activeMeshes.Contains(meshName)) {
                meshesToLoad.pop_back();
                continue;
            }

            activeMeshes.Register(meshName, make_shared<Mesh>(model, meshIndex, *this, device));
            meshesToLoad.pop_back();
        }
    }

    GPUScene::DrawBufferIDs GPUScene::GenerateDrawsForView(rg::RenderGraph &graph,
        ecs::Renderable::VisibilityMask viewMask) {
        DrawBufferIDs bufferIDs;

        graph.AddPass("GenerateDrawsForView")
            .Build([&](rg::PassBuilder &builder) {
                const auto maxDraws = primitiveCountPowerOfTwo;

                auto drawCmds = builder.StorageCreate(sizeof(uint32) + maxDraws * sizeof(VkDrawIndexedIndirectCommand),
                    Residency::GPU_ONLY);
                builder.TransferWrite(drawCmds.id);
                bufferIDs.drawCommandsBuffer = drawCmds.id;

                auto drawParams = builder.StorageCreate(maxDraws * sizeof(uint16) * 2, Residency::GPU_ONLY);
                bufferIDs.drawParamsBuffer = drawParams.id;

                builder.StorageRead("RenderableEntities", rg::PipelineStage::eComputeShader);
            })
            .Execute([this, viewMask, bufferIDs](rg::Resources &resources, CommandContext &cmd) {
                auto drawBuffer = resources.GetBuffer(bufferIDs.drawCommandsBuffer);
                cmd.Raw().fillBuffer(*drawBuffer, 0, sizeof(uint32), 0);

                cmd.SetComputeShader("generate_draws_for_view.comp");
                cmd.SetStorageBuffer(0, 0, resources.GetBuffer("RenderableEntities"));
                cmd.SetStorageBuffer(0, 1, models);
                cmd.SetStorageBuffer(0, 2, primitiveLists);
                cmd.SetStorageBuffer(0, 3, drawBuffer);
                cmd.SetStorageBuffer(0, 4, resources.GetBuffer(bufferIDs.drawParamsBuffer));

                struct {
                    uint32 renderableCount;
                    uint32 visibilityMask;
                } constants;
                constants.renderableCount = renderableCount;
                constants.visibilityMask = viewMask.to_ulong();
                cmd.PushConstants(constants);

                cmd.Dispatch((renderableCount + 127) / 128, 1, 1);
            });
        return bufferIDs;
    }

    void GPUScene::DrawSceneIndirect(CommandContext &cmd,
        BufferPtr vertexBuffer,
        BufferPtr drawCommandsBuffer,
        BufferPtr drawParamsBuffer) {
        if (vertexCount == 0) return;

        cmd.SetBindlessDescriptors(2, textures.GetDescriptorSet());

        cmd.SetVertexLayout(SceneVertex::Layout());
        cmd.Raw().bindIndexBuffer(*indexBuffer, 0, vk::IndexType::eUint32);
        cmd.Raw().bindVertexBuffers(0, {*vertexBuffer}, {0});

        if (drawParamsBuffer) cmd.SetStorageBuffer(1, 0, drawParamsBuffer);
        cmd.DrawIndexedIndirectCount(drawCommandsBuffer,
            sizeof(uint32),
            drawCommandsBuffer,
            0,
            drawCommandsBuffer->Size() / sizeof(VkDrawIndexedIndirectCommand));
    }

    void GPUScene::AddGeometryWarp(rg::RenderGraph &graph) {
        graph.AddPass("GeometryWarp")
            .Build([&](rg::PassBuilder &builder) {
                const auto maxDraws = primitiveCountPowerOfTwo;

                builder.BufferCreate("WarpedVertexDrawCmds",
                    sizeof(uint32) + maxDraws * sizeof(VkDrawIndirectCommand),
                    rg::BufferUsage::eStorageBuffer | rg::BufferUsage::eIndirectBuffer | rg::BufferUsage::eTransferDst,
                    Residency::GPU_ONLY);

                builder.StorageCreate("WarpedVertexDrawParams", maxDraws * sizeof(glm::vec4) * 5, Residency::GPU_ONLY);

                builder.BufferCreate("WarpedVertexBuffer",
                    sizeof(SceneVertex) * vertexCount,
                    rg::BufferUsage::eStorageBuffer | rg::BufferUsage::eVertexBuffer,
                    Residency::GPU_ONLY);

                builder.StorageRead("RenderableEntities", rg::PipelineStage::eComputeShader);
            })
            .Execute([this](rg::Resources &resources, CommandContext &cmd) {
                if (vertexCount == 0) return;
                auto renderableEntityList = resources.GetBuffer("RenderableEntities");

                vk::BufferMemoryBarrier barrier;
                barrier.srcAccessMask = vk::AccessFlagBits::eHostWrite;
                barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
                barrier.buffer = *renderableEntityList;
                barrier.size = VK_WHOLE_SIZE;
                cmd.Raw().pipelineBarrier(vk::PipelineStageFlagBits::eHost,
                    vk::PipelineStageFlagBits::eComputeShader,
                    {},
                    {},
                    {barrier},
                    {});

                auto cmdBuffer = resources.GetBuffer("WarpedVertexDrawCmds");
                auto paramBuffer = resources.GetBuffer("WarpedVertexDrawParams");
                auto warpedVertexBuffer = resources.GetBuffer("WarpedVertexBuffer");

                cmd.Raw().fillBuffer(*cmdBuffer, 0, sizeof(uint32), 0);

                cmd.SetComputeShader("generate_warp_geometry_draws.comp");
                cmd.SetStorageBuffer(0, 0, renderableEntityList);
                cmd.SetStorageBuffer(0, 1, models);
                cmd.SetStorageBuffer(0, 2, primitiveLists);
                cmd.SetStorageBuffer(0, 3, cmdBuffer);
                cmd.SetStorageBuffer(0, 4, paramBuffer);

                struct {
                    uint32 renderableCount;
                } constants;
                constants.renderableCount = renderableCount;
                cmd.PushConstants(constants);
                cmd.Dispatch((renderableCount + 127) / 128, 1, 1);

                barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
                barrier.dstAccessMask = vk::AccessFlagBits::eIndirectCommandRead;
                barrier.buffer = *cmdBuffer;
                barrier.size = VK_WHOLE_SIZE;
                cmd.Raw().pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                    vk::PipelineStageFlagBits::eDrawIndirect,
                    {},
                    {},
                    {barrier},
                    {});

                barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
                barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
                barrier.buffer = *paramBuffer;
                barrier.size = VK_WHOLE_SIZE;
                cmd.Raw().pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                    vk::PipelineStageFlagBits::eVertexShader,
                    {},
                    {},
                    {barrier},
                    {});

                cmd.BeginRenderPass({});
                cmd.SetShaders({{ShaderStage::Vertex, "warp_geometry.vert"}});
                cmd.SetStorageBuffer(0, 0, paramBuffer);
                cmd.SetStorageBuffer(0, 1, warpedVertexBuffer);
                cmd.SetVertexLayout(SceneVertex::Layout());
                cmd.SetPrimitiveTopology(vk::PrimitiveTopology::ePointList);
                cmd.Raw().bindVertexBuffers(0, {*vertexBuffer}, {0});
                cmd.DrawIndirect(cmdBuffer, sizeof(uint32), primitiveCount);
                cmd.EndRenderPass();
            });
    }

} // namespace sp::vulkan
