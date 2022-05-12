#include "GPUScene.hh"

#include "assets/GltfImpl.hh"
#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/scene/Mesh.hh"
#include "graphics/vulkan/scene/VertexLayouts.hh"

namespace sp::vulkan {
    GPUScene::GPUScene(DeviceContext &device) : device(device), workQueue("", 0), textures(device, workQueue) {
        indexBuffer = device.AllocateBuffer({sizeof(uint32), 10 * 1024 * 1024},
            vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
            VMA_MEMORY_USAGE_GPU_ONLY);

        vertexBuffer = device.AllocateBuffer({sizeof(SceneVertex), 1024 * 1024},
            vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
            VMA_MEMORY_USAGE_GPU_ONLY);

        jointsBuffer = device.AllocateBuffer({sizeof(JointVertex), 128 * 1024},
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
            VMA_MEMORY_USAGE_GPU_ONLY);

        primitiveLists = device.AllocateBuffer({sizeof(GPUMeshPrimitive), 10 * 1024},
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
            VMA_MEMORY_USAGE_GPU_ONLY);

        models = device.AllocateBuffer({sizeof(GPUMeshModel), 1024},
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
            VMA_MEMORY_USAGE_GPU_ONLY);
    }

    void GPUScene::Flush() {
        workQueue.Flush();
        textures.Flush();
        FlushMeshes();
    }

    void GPUScene::LoadState(rg::RenderGraph &graph,
        ecs::Lock<ecs::Read<ecs::Renderable, ecs::TransformSnapshot, ecs::Name>> lock) {
        ZoneScoped;
        renderables.clear();
        opticEntities.clear();
        jointPoses.clear();
        renderableCount = 0;
        primitiveCount = 0;
        vertexCount = 0;

        for (auto &ent : lock.EntitiesWith<ecs::Renderable>()) {
            if (!ent.Has<ecs::TransformSnapshot>(lock)) continue;

            auto &renderable = ent.Get<ecs::Renderable>(lock);
            if (!renderable.model || !renderable.model->Ready()) continue;

            auto model = renderable.model->Get();
            if (!model) continue;

            auto vkMesh = LoadMesh(model, renderable.meshIndex);
            if (!vkMesh || !vkMesh->CheckReady()) continue;

            GPURenderableEntity gpuRenderable;
            gpuRenderable.modelToWorld = ent.Get<ecs::TransformSnapshot>(lock).matrix;
            gpuRenderable.visibilityMask = renderable.visibility.to_ulong();
            gpuRenderable.meshIndex = vkMesh->SceneIndex();
            gpuRenderable.vertexOffset = vertexCount;
            if (ent.Has<ecs::OpticalElement>(lock)) {
                opticEntities.emplace_back(ent);
                gpuRenderable.opticID = opticEntities.size();
            }

            if (!renderable.joints.empty()) gpuRenderable.jointPosesOffset = jointPoses.size();

            for (auto &joint : renderable.joints) {
                auto jointEntity = joint.entity.Get(lock);
                if (jointEntity.Has<ecs::TransformSnapshot>(lock)) {
                    auto &jointTransform = jointEntity.Get<ecs::TransformSnapshot>(lock);
                    jointPoses.push_back(glm::mat4(jointTransform.matrix) * joint.inverseBindPose);
                } else {
                    jointPoses.emplace_back(); // missing joints get an identity matrix
                }
            }

            renderables.push_back(gpuRenderable);
            renderableCount++;
            primitiveCount += vkMesh->PrimitiveCount();
            vertexCount += vkMesh->VertexCount();
        }

        primitiveCountPowerOfTwo = std::max(1u, CeilToPowerOfTwo(primitiveCount));

        graph.AddPass("SceneState")
            .Build([&](rg::PassBuilder &builder) {
                builder.CreateBuffer("RenderableEntities",
                    {sizeof(renderables.front()), std::max(size_t(1), renderables.size())},
                    Residency::CPU_TO_GPU,
                    Access::HostWrite);

                Assertf(jointPoses.size() <= 100, "too many joints: %d", jointPoses.size());
                builder.CreateUniform("JointPoses", sizeof(glm::mat4) * 100); // TODO: don't hardcode to 100 joints
            })
            .Execute([this](rg::Resources &resources, DeviceContext &device) {
                resources.GetBuffer("RenderableEntities")->CopyFrom(renderables.data(), renderables.size());
                resources.GetBuffer("JointPoses")->CopyFrom(jointPoses.data(), jointPoses.size());
            });
    }

    shared_ptr<Mesh> GPUScene::LoadMesh(const std::shared_ptr<const sp::Gltf> &model, size_t meshIndex) {
        auto vkMesh = activeMeshes.Load(MeshKeyView{model->name, meshIndex});
        if (!vkMesh) { meshesToLoad.emplace_back(model, meshIndex); }
        return vkMesh;
    }

    void GPUScene::FlushMeshes() {
        activeMeshes.Tick(std::chrono::milliseconds(33));

        for (int i = (int)meshesToLoad.size() - 1; i >= 0; i--) {
            auto &[model, meshIndex] = meshesToLoad[i];
            if (activeMeshes.Contains(MeshKeyView{model->name, meshIndex})) {
                meshesToLoad.pop_back();
                continue;
            }

            activeMeshes.Register(MeshKey{model->name, meshIndex}, make_shared<Mesh>(model, meshIndex, *this, device));
            meshesToLoad.pop_back();
        }
    }

    GPUScene::DrawBufferIDs GPUScene::GenerateDrawsForView(rg::RenderGraph &graph,
        ecs::Renderable::VisibilityMask viewMask,
        uint32 instanceCount) {
        DrawBufferIDs bufferIDs;

        graph.AddPass("GenerateDrawsForView")
            .Build([&](rg::PassBuilder &builder) {
                const auto maxDraws = primitiveCountPowerOfTwo;

                graph.AddPass("Clear")
                    .Build([&](rg::PassBuilder &builder) {
                        auto drawCmds = builder.CreateBuffer(
                            {sizeof(uint32), sizeof(VkDrawIndexedIndirectCommand), maxDraws},
                            Residency::GPU_ONLY,
                            Access::TransferWrite);
                        bufferIDs.drawCommandsBuffer = drawCmds.id;
                    })
                    .Execute([bufferIDs](rg::Resources &resources, CommandContext &cmd) {
                        auto drawBuffer = resources.GetBuffer(bufferIDs.drawCommandsBuffer);
                        cmd.Raw().fillBuffer(*drawBuffer, 0, sizeof(uint32), 0);
                    });

                builder.Read("RenderableEntities", Access::ComputeShaderReadStorage);
                builder.Write(bufferIDs.drawCommandsBuffer, Access::ComputeShaderWrite);

                auto drawParams = builder.CreateBuffer({sizeof(uint16) * 3, maxDraws},
                    Residency::GPU_ONLY,
                    Access::ComputeShaderWrite);
                bufferIDs.drawParamsBuffer = drawParams.id;
            })
            .Execute([this, viewMask, bufferIDs, instanceCount](rg::Resources &resources, CommandContext &cmd) {
                cmd.SetComputeShader("generate_draws_for_view.comp");
                cmd.SetStorageBuffer(0, 0, resources.GetBuffer("RenderableEntities"));
                cmd.SetStorageBuffer(0, 1, models);
                cmd.SetStorageBuffer(0, 2, primitiveLists);
                cmd.SetStorageBuffer(0, 3, resources.GetBuffer(bufferIDs.drawCommandsBuffer));
                cmd.SetStorageBuffer(0, 4, resources.GetBuffer(bufferIDs.drawParamsBuffer));

                struct {
                    uint32 renderableCount;
                    uint32 instanceCount;
                    uint32 visibilityMask;
                } constants;
                constants.renderableCount = renderableCount;
                constants.instanceCount = instanceCount;
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
            drawCommandsBuffer->ArraySize());
    }

    void GPUScene::AddGeometryWarp(rg::RenderGraph &graph) {
        graph.AddPass("GeometryWarpCalls")
            .Build([&](rg::PassBuilder &builder) {
                const auto maxDraws = primitiveCountPowerOfTwo;

                graph.AddPass("Clear")
                    .Build([&](rg::PassBuilder &builder) {
                        builder.CreateBuffer("WarpedVertexDrawCmds",
                            {sizeof(uint32), sizeof(VkDrawIndirectCommand), maxDraws},
                            Residency::GPU_ONLY,
                            Access::TransferWrite);
                    })
                    .Execute([](rg::Resources &resources, CommandContext &cmd) {
                        cmd.Raw().fillBuffer(*resources.GetBuffer("WarpedVertexDrawCmds"), 0, sizeof(uint32), 0);
                    });

                builder.Read("RenderableEntities", Access::ComputeShaderReadStorage);
                builder.Write("WarpedVertexDrawCmds", Access::ComputeShaderWrite);

                builder.CreateBuffer("WarpedVertexDrawParams",
                    {sizeof(glm::vec4) * 5, maxDraws},
                    Residency::GPU_ONLY,
                    Access::ComputeShaderWrite);
            })
            .Execute([this](rg::Resources &resources, CommandContext &cmd) {
                if (vertexCount == 0) return;

                cmd.SetComputeShader("generate_warp_geometry_draws.comp");
                cmd.SetStorageBuffer(0, 0, resources.GetBuffer("RenderableEntities"));
                cmd.SetStorageBuffer(0, 1, models);
                cmd.SetStorageBuffer(0, 2, primitiveLists);
                cmd.SetStorageBuffer(0, 3, resources.GetBuffer("WarpedVertexDrawCmds"));
                cmd.SetStorageBuffer(0, 4, resources.GetBuffer("WarpedVertexDrawParams"));

                struct {
                    uint32 renderableCount;
                } constants;
                constants.renderableCount = renderableCount;
                cmd.PushConstants(constants);
                cmd.Dispatch((renderableCount + 127) / 128, 1, 1);
            });

        graph.AddPass("GeometryWarp")
            .Build([&](rg::PassBuilder &builder) {
                builder.Read("WarpedVertexDrawCmds", Access::IndirectBuffer);
                builder.Read("WarpedVertexDrawParams", Access::VertexShaderReadStorage);
                builder.Read("JointPoses", Access::VertexShaderReadUniform);

                builder.CreateBuffer("WarpedVertexBuffer",
                    {sizeof(SceneVertex), std::max(1u, vertexCount)},
                    Residency::GPU_ONLY,
                    Access::VertexShaderWrite);
            })
            .Execute([this](rg::Resources &resources, CommandContext &cmd) {
                if (vertexCount == 0) return;

                auto cmdBuffer = resources.GetBuffer("WarpedVertexDrawCmds");
                auto paramBuffer = resources.GetBuffer("WarpedVertexDrawParams");
                auto warpedVertexBuffer = resources.GetBuffer("WarpedVertexBuffer");

                cmd.BeginRenderPass({});
                cmd.SetShaders({{ShaderStage::Vertex, "warp_geometry.vert"}});
                cmd.SetStorageBuffer(0, 0, paramBuffer);
                cmd.SetStorageBuffer(0, 1, warpedVertexBuffer);
                cmd.SetUniformBuffer(0, 2, resources.GetBuffer("JointPoses"));
                cmd.SetStorageBuffer(0, 3, jointsBuffer);

                cmd.SetVertexLayout(SceneVertex::Layout());
                cmd.SetPrimitiveTopology(vk::PrimitiveTopology::ePointList);
                cmd.Raw().bindVertexBuffers(0, {*vertexBuffer}, {0});
                cmd.DrawIndirect(cmdBuffer, sizeof(uint32), primitiveCount);
                cmd.EndRenderPass();
            });
    }

} // namespace sp::vulkan
