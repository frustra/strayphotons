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
        meshes.clear();
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

            auto &transform = ent.Get<ecs::TransformSnapshot>(lock);

            GPURenderableEntity gpuRenderable;
            gpuRenderable.modelToWorld = transform.matrix;
            gpuRenderable.visibilityMask = (uint32_t)renderable.visibility;
            gpuRenderable.meshIndex = vkMesh->SceneIndex();
            gpuRenderable.vertexOffset = vertexCount;
            gpuRenderable.emissiveScale = renderable.emissiveScale;
            if (ent.Has<ecs::OpticalElement>(lock)) {
                opticEntities.emplace_back(ent);
                gpuRenderable.opticID = opticEntities.size();
                gpuRenderable.visibilityMask |= (uint32_t)ecs::VisibilityMask::Optics;
            } else {
                gpuRenderable.visibilityMask &= (uint32_t)~ecs::VisibilityMask::Optics;
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
            meshes.emplace_back(vkMesh);

            renderableCount++;
            primitiveCount += vkMesh->PrimitiveCount();
            vertexCount += vkMesh->VertexCount();
        }

        Assertf(renderables.size() == meshes.size(),
            "Mismatched renderable and mesh counts: %u != %u",
            renderables.size(),
            meshes.size());

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
        if (meshIndex >= model->meshes.size()) return nullptr;
        auto vkMesh = activeMeshes.Load(MeshKeyView{model->name, meshIndex});
        if (!vkMesh) {
            meshesToLoad.emplace_back(model, meshIndex);
        }
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
        ecs::VisibilityMask viewMask,
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
                builder.Read(bufferIDs.drawCommandsBuffer, Access::ComputeShaderReadStorage);
                builder.Write(bufferIDs.drawCommandsBuffer, Access::ComputeShaderWrite);

                auto drawParams = builder.CreateBuffer({sizeof(GPUDrawParams), maxDraws},
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
                constants.visibilityMask = (uint32_t)viewMask;
                cmd.PushConstants(constants);

                cmd.Dispatch((renderableCount + 127) / 128, 1, 1);
            });
        return bufferIDs;
    }

    GPUScene::DrawBufferIDs GPUScene::GenerateSortedDrawsForView(rg::RenderGraph &graph,
        glm::vec3 viewPosition,
        ecs::VisibilityMask viewMask,
        bool reverseSort,
        uint32 instanceCount) {
        DrawBufferIDs bufferIDs;

        graph.AddPass("GenerateSortedDrawsForView")
            .Build([&](rg::PassBuilder &builder) {
                const auto maxDraws = primitiveCountPowerOfTwo;

                auto drawCmds = builder.CreateBuffer({sizeof(uint32), sizeof(VkDrawIndexedIndirectCommand), maxDraws},
                    Residency::CPU_TO_GPU,
                    Access::HostWrite);
                bufferIDs.drawCommandsBuffer = drawCmds.id;

                auto drawParams = builder.CreateBuffer({sizeof(GPUDrawParams), maxDraws},
                    Residency::CPU_TO_GPU,
                    Access::HostWrite);
                bufferIDs.drawParamsBuffer = drawParams.id;
            })
            .Execute([this, viewMask, viewPosition, bufferIDs, instanceCount, reverseSort](rg::Resources &resources,
                         CommandContext &cmd) {
                InlineVector<VkDrawIndexedIndirectCommand, 10 * 1024> drawCommands;
                InlineVector<GPUDrawParams, 10 * 1024> drawParams;
                InlineVector<float, 10 * 1024> primitiveDepth;

                for (size_t i = 0; i < renderables.size(); i++) {
                    auto &renderable = renderables[i];
                    if (((ecs::VisibilityMask)renderable.visibilityMask & viewMask) != viewMask) continue;

                    auto mesh = meshes[i].lock();
                    if (!mesh || !mesh->CheckReady()) continue;

                    for (auto &primitive : mesh->primitives) {
                        auto &drawCmd = drawCommands.emplace_back();

                        drawCmd.indexCount = primitive.indexCount;
                        drawCmd.instanceCount = instanceCount;
                        drawCmd.firstIndex = mesh->indexBuffer->ArrayOffset() + primitive.indexOffset;
                        drawCmd.vertexOffset = renderable.vertexOffset + primitive.vertexOffset;

                        drawCmd.firstInstance = drawParams.size();
                        auto &drawParam = drawParams.emplace_back();

                        drawParam.baseColorTexID = primitive.baseColor.index;
                        drawParam.metallicRoughnessTexID = primitive.metallicRoughness.index;
                        drawParam.opticID = renderable.opticID;
                        drawParam.emissiveScale = renderable.emissiveScale;

                        auto worldPos = renderable.modelToWorld * glm::vec4(primitive.center, 1);
                        auto relPos = (glm::vec3(worldPos) / worldPos.w) - viewPosition;
                        primitiveDepth.push_back(glm::length(relPos));
                    }
                }

                if (reverseSort) {
                    // Sort primitives farthest first
                    std::sort(drawCommands.begin(), drawCommands.end(), [&](auto a, auto b) {
                        return primitiveDepth[a.firstInstance] > primitiveDepth[b.firstInstance];
                    });
                } else {
                    // Sort primitives nearest first
                    std::sort(drawCommands.begin(), drawCommands.end(), [&](auto a, auto b) {
                        return primitiveDepth[a.firstInstance] < primitiveDepth[b.firstInstance];
                    });
                }

                auto commandsBuffer = resources.GetBuffer(bufferIDs.drawCommandsBuffer);
                uint32_t *cmdBufferPtr = nullptr;
                commandsBuffer->Map((void **)&cmdBufferPtr);
                cmdBufferPtr[0] = drawCommands.size();
                std::copy_n(drawCommands.data(),
                    drawCommands.size(),
                    reinterpret_cast<VkDrawIndexedIndirectCommand *>(cmdBufferPtr + 1));
                commandsBuffer->Unmap();
                commandsBuffer->Flush();

                auto paramsBuffer = resources.GetBuffer(bufferIDs.drawParamsBuffer);
                paramsBuffer->CopyFrom(drawParams.data(), drawParams.size());
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
                builder.Read("WarpedVertexDrawCmds", Access::ComputeShaderReadStorage);
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
