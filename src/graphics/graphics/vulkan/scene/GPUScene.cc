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

    void GPUScene::LoadState(ecs::Lock<ecs::Read<ecs::Renderable, ecs::TransformSnapshot>> lock) {
        renderableCount = 0;
        primitiveCount = 0;
        vertexCount = 0;

        renderableEntityList = device.GetFramePooledBuffer(BUFFER_TYPE_STORAGE_TRANSFER, 1024 * 1024);

        auto gpuRenderable = (GPURenderableEntity *)renderableEntityList->Mapped();

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

            Assert(renderableCount * sizeof(GPURenderableEntity) < renderableEntityList->Size(),
                "renderable entity overflow");

            gpuRenderable->modelToWorld = ent.Get<ecs::TransformSnapshot>(lock).matrix;
            gpuRenderable->visibilityMask = renderable.visibility.to_ulong();
            gpuRenderable->meshIndex = vkMesh->SceneIndex();
            gpuRenderable->vertexOffset = vertexCount;
            gpuRenderable++;
            renderableCount++;
            primitiveCount += vkMesh->PrimitiveCount();
            vertexCount += vkMesh->VertexCount();
        }

        primitiveCountPowerOfTwo = std::max(1u, CeilToPowerOfTwo(primitiveCount));
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

                auto drawCmds = builder.CreateBuffer(BUFFER_TYPE_STORAGE_LOCAL_INDIRECT,
                    sizeof(uint32) + maxDraws * sizeof(VkDrawIndexedIndirectCommand));
                bufferIDs.drawCommandsBuffer = drawCmds.id;

                auto drawParams = builder.CreateBuffer(BUFFER_TYPE_STORAGE_LOCAL, maxDraws * sizeof(uint16) * 2);
                bufferIDs.drawParamsBuffer = drawParams.id;
            })
            .Execute([this, viewMask, bufferIDs](rg::Resources &resources, CommandContext &cmd) {
                auto drawBuffer = resources.GetBuffer(bufferIDs.drawCommandsBuffer);
                cmd.Raw().fillBuffer(*drawBuffer, 0, sizeof(uint32), 0);

                cmd.SetComputeShader("generate_draws_for_view.comp");
                cmd.SetStorageBuffer(0, 0, renderableEntityList);
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

} // namespace sp::vulkan
