/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justine Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "GPUScene.hh"

#include "assets/Asset.hh"
#include "assets/GltfImpl.hh"
#include "common/Tracing.hh"
#include "ecs/Ecs.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityRef.hh"
#include "ecs/components/Light.hh"
#include "ecs/components/Renderable.hh"
#include "game/Scene.hh"
#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/render_graph/Resources.hh"
#include "graphics/vulkan/scene/Mesh.hh"
#include "graphics/vulkan/scene/VertexLayouts.hh"
#include "strayphotons/HeapVector.hh"
#include "strayphotons/Logging.hh"
#include "strayphotons/Utility.hh"

#include <chrono>
#include <limits>
#include <memory>
#include <string>
#include <tracy/Tracy.hpp>

namespace sp::vulkan {
    GPUScene::GPUScene(DeviceContext &device)
        : device(device), textures(device), workQueue("GPUScene", 0, std::chrono::milliseconds(1)) {
        indexBuffer = device.AllocateBuffer({sizeof(uint32_t), 64 * 1024 * 1024},
            vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
            VMA_MEMORY_USAGE_GPU_ONLY);

        vertexBuffer = device.AllocateBuffer({sizeof(SceneVertex), 16 * 1024 * 1024},
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

        auto lock = ecs::StartTransaction<ecs::AddRemove>();
        renderableObserver = lock.Watch<ecs::ComponentModifiedEvent<ecs::Renderable>>();
        lightObserver = lock.Watch<ecs::ComponentModifiedEvent<ecs::Light>>();
    }

    GPUScene::OpticInstance::OpticInstance(ecs::Entity ent, const ecs::OpticalElement &optic) : ent(ent) {
        pass = optic.passTint != glm::vec3(0);
        reflect = optic.reflectTint != glm::vec3(0);
    }

    void GPUScene::Flush() {
        textures.Flush();
        FlushMeshes();
    }

    RenderableIndex GPUScene::AllocateRenderableIndex(ecs::Entity ent) {
        DebugAssertf(!sp::contains(gpuRenderableEntities, ent),
            "GPURenderableEntity %s already exists",
            ecs::EntityRef(ent).Name().String());
        RenderableIndex index = gpuRenderables.size();
        gpuRenderables.emplace_back();
        gpuRenderableEntities.emplace_back(ent);
        renderablesToFlush.emplace_back(index);
        return index;
    }

    void GPUScene::ReleaseRenderable(RenderableIndex index) {
        if (index >= gpuRenderables.size()) return;
        // Deallocate the renderable index by swapping it with the last renderable slot
        std::swap(gpuRenderables[index], gpuRenderables.back());
        std::swap(gpuRenderableEntities[index], gpuRenderableEntities.back());
        gpuRenderables.pop_back();
        gpuRenderableEntities.pop_back();
        renderablesToFlush.emplace_back(index);
        if (index < gpuRenderableEntities.size()) {
            const ecs::Entity &swappedEnt = gpuRenderableEntities[index];
            DebugAssertf(liveEntityState[swappedEnt].renderableIndex == gpuRenderables.size(),
                "Unexpected swapped renderable index");
            liveEntityState[swappedEnt].renderableIndex = index;
        }
    }

    void GPUScene::LoadState(rg::RenderGraph &graph,
        ecs::Lock<ecs::Read<ecs::Renderable, ecs::Light, ecs::OpticalElement, ecs::TransformSnapshot, ecs::Name>>
            lock) {
        ZoneScoped;
        DebugAssertf(ecs::IsLive(lock), "GPUScene::LoadState expects live ecs lock");
        opticEntities.clear();
        jointPoses.clear();
        primitiveCount = 0;
        vertexCount = 0;

        {
            ZoneScopedN("UpdateModified");
            ecs::ComponentModifiedEvent<ecs::Renderable> renderableModifiedEvent;
            while (renderableObserver.Poll(lock, renderableModifiedEvent)) {
                if (!renderableModifiedEvent.Exists(lock)) entitiesPendingDelete.emplace_back(renderableModifiedEvent);
                if (renderableModifiedEvent.Has<ecs::Renderable>(lock)) {
                    auto &state = liveEntityState[renderableModifiedEvent];
                    auto &renderable = renderableModifiedEvent.Get<const ecs::Renderable>(lock);
                    if (renderable.modelName != state.renderableModelName ||
                        renderable.meshIndex != state.renderableMeshIndex) {
                        state.renderableModelName = renderable.modelName;
                        state.renderableMeshIndex = renderable.meshIndex;
                        if (!renderable.modelName.empty()) {
                            state.renderableMesh = LoadMesh(renderable);
                            if (state.renderableMesh) {
                                if (state.renderableIndex < gpuRenderables.size()) {
                                    // Reuse existing index
                                    renderablesToFlush.emplace_back(state.renderableIndex);
                                } else {
                                    state.renderableIndex = AllocateRenderableIndex(renderableModifiedEvent);
                                }
                            } else {
                                Errorf("Renderable %s mesh is null: %s",
                                    ecs::ToString(lock, renderableModifiedEvent),
                                    renderable.modelName);
                                ReleaseRenderable(state.renderableIndex);
                                state.renderableIndex = std::numeric_limits<RenderableIndex>::max();
                            }
                        } else {
                            ReleaseRenderable(state.renderableIndex);
                            state.renderableIndex = std::numeric_limits<RenderableIndex>::max();
                        }
                    }
                    if (renderable.textureOverrideName != state.renderableTextureOverrideName) {
                        state.renderableTextureOverrideName = renderable.textureOverrideName;
                        state.renderableTextureOverride = textures.LoadResource(renderable.textureOverrideName);
                    }
                } else if (liveEntityState.count(renderableModifiedEvent) > 0) {
                    auto &state = liveEntityState[renderableModifiedEvent];
                    state.renderableModelName = "";
                    state.renderableMeshIndex = std::numeric_limits<MeshIndex>::max();
                    state.renderableMesh.reset();
                    ReleaseRenderable(state.renderableIndex);
                    state.renderableIndex = std::numeric_limits<RenderableIndex>::max();
                    state.renderableTextureOverrideName = "";
                    state.renderableTextureOverride = {};
                }
            }
            ecs::ComponentModifiedEvent<ecs::Light> lightModifiedEvent;
            while (lightObserver.Poll(lock, lightModifiedEvent)) {
                if (!lightModifiedEvent.Exists(lock)) entitiesPendingDelete.emplace_back(lightModifiedEvent);
                if (lightModifiedEvent.Has<ecs::Light>(lock)) {
                    auto &state = liveEntityState[lightModifiedEvent];
                    auto &light = lightModifiedEvent.Get<const ecs::Light>(lock);
                    if (light.filterName != state.lightFilterName) {
                        state.lightFilterName = light.filterName;
                        state.lightFilter = textures.LoadResource(light.filterName);
                    }
                } else if (liveEntityState.count(lightModifiedEvent) > 0) {
                    auto &state = liveEntityState[lightModifiedEvent];
                    state.lightFilterName = "";
                    state.lightFilter = {};
                }
            }
            for (const ecs::Entity &ent : entitiesPendingDelete) {
                liveEntityState.erase(ent);
            }
            entitiesPendingDelete.clear();
        }

        {
            // TODO: Use observer on renderable + TransformSnapshot
            ZoneScopedN("RefreshGPURenderables");
            for (const auto &pair : liveEntityState) {
                const ecs::Entity &ent = pair.first;
                const EntityState &state = pair.second;
                if (!ent.Has<ecs::Renderable, ecs::TransformSnapshot>(lock)) continue;
                if (state.renderableIndex >= gpuRenderables.size()) continue;
                if (!state.renderableMesh || !state.renderableMesh->Ready()) continue;
                std::shared_ptr<Mesh> vkMesh = state.renderableMesh->Get();
                if (!vkMesh || !vkMesh->Valid() || !vkMesh->CheckReady()) continue;

                auto &renderable = ent.Get<ecs::Renderable>(lock);
                auto &transform = ent.Get<ecs::TransformSnapshot>(lock).globalPose;
                GPURenderableEntity &gpuRenderable = gpuRenderables[state.renderableIndex];
                gpuRenderable.modelToWorld = transform.GetMatrix();
                gpuRenderable.visibilityMask = (uint32_t)renderable.visibility;
                gpuRenderable.meshIndex = vkMesh->SceneIndex();
                gpuRenderable.vertexOffset = vertexCount;
                gpuRenderable.emissiveScale = renderable.emissiveScale;
                if (!renderable.textureOverrideName.empty()) {
                    if (state.renderableTextureOverride) {
                        gpuRenderable.baseColorOverrideID = state.renderableTextureOverride.index;
                    } else {
                        gpuRenderable.baseColorOverrideID = textures.GetSinglePixelIndex(ERROR_COLOR);
                    }
                } else if (glm::all(glm::greaterThanEqual(renderable.colorOverride.color, glm::vec4(0)))) {
                    gpuRenderable.baseColorOverrideID = textures.GetSinglePixelIndex(renderable.colorOverride);
                }
                if (glm::all(glm::greaterThanEqual(renderable.metallicRoughnessOverride, glm::vec2(0)))) {
                    gpuRenderable.metallicRoughnessOverrideID = textures.GetSinglePixelIndex(glm::vec4(0,
                        renderable.metallicRoughnessOverride.g,
                        renderable.metallicRoughnessOverride.r,
                        1));
                }
                if (ent.Has<ecs::OpticalElement>(lock)) {
                    auto &optic = ent.Get<ecs::OpticalElement>(lock);
                    opticEntities.emplace_back(ent, optic);
                    gpuRenderable.opticID = opticEntities.size();
                    gpuRenderable.visibilityMask |= (uint32_t)ecs::VisibilityMask::Optics;
                } else {
                    gpuRenderable.visibilityMask &= (uint32_t)~ecs::VisibilityMask::Optics;
                }

                if (!renderable.joints.empty()) gpuRenderable.jointPosesOffset = jointPoses.size();

                for (auto &joint : renderable.joints) {
                    auto jointEntity = joint.entity.Get(lock);
                    if (jointEntity.Has<ecs::TransformSnapshot>(lock)) {
                        auto &jointTransform = jointEntity.Get<ecs::TransformSnapshot>(lock).globalPose;
                        jointPoses.push_back(jointTransform.GetMatrix() * joint.inverseBindPose);
                    } else {
                        jointPoses.emplace_back(); // missing joints get an identity matrix
                    }
                }

                primitiveCount += vkMesh->PrimitiveCount();
                vertexCount += vkMesh->VertexCount();
            }
        }

        Assertf(gpuRenderables.size() == gpuRenderableEntities.size(),
            "Mismatched renderable and entity counts: %llu != %llu",
            gpuRenderables.size(),
            gpuRenderableEntities.size());

        primitiveCountPowerOfTwo = CeilToPowerOfTwo(primitiveCount);

        textures.Flush();

        // TODO: Reuse or modify the previous frame's buffer to reduce upload bandwidth
        renderablesToFlush.clear();
        graph.AddPass("SceneState")
            .Build([&](rg::PassBuilder &builder) {
                builder.CreateBuffer("RenderableEntities",
                    {sizeof(gpuRenderables.front()), std::max(size_t(1), gpuRenderables.size())},
                    Residency::CPU_TO_GPU,
                    Access::HostWrite);

                Assertf(jointPoses.size() <= 100, "too many joints: %d", jointPoses.size());
                builder.CreateUniform("JointPoses", sizeof(glm::mat4) * 100); // TODO: don't hardcode to 100 joints
            })
            .Execute([this](rg::Resources &resources, DeviceContext &device) {
                resources.GetBuffer("RenderableEntities")->CopyFrom(gpuRenderables.data(), gpuRenderables.size());
                resources.GetBuffer("JointPoses")->CopyFrom(jointPoses.data(), jointPoses.size());
            });
    }

    bool GPUScene::PreloadScene(
        ecs::Lock<ecs::Read<ecs::Name, ecs::SceneInfo, ecs::Renderable, ecs::Light, ecs::RenderOutput, ecs::Screen>>
            lock,
        std::shared_ptr<Scene> scene) {
        ZoneScoped;
        Assertf(ecs::IsStaging(lock), "GPUScene::PreloadScene expects staging ecs lock");
        bool complete = true;
        // TODO: Switch to observer pattern, iterating this each frame is inefficient
        HeapVector<ecs::Entity> entitiesToRemove;
        for (const auto &pair : stagingEntityState) {
            if (!pair.first.Exists(lock)) {
                entitiesToRemove.emplace_back(pair.first);
                continue;
            }
            auto &state = stagingEntityState[pair.first];
            if (pair.first.Has<ecs::Renderable>(lock)) {
                auto &renderable = pair.first.Get<const ecs::Renderable>(lock);
                if (renderable.modelName != state.renderableModelName ||
                    renderable.meshIndex != state.renderableMeshIndex) {
                    state.renderableModelName = renderable.modelName;
                    state.renderableMeshIndex = renderable.meshIndex;
                    if (!renderable.modelName.empty()) {
                        state.renderableMesh = LoadMesh(renderable);
                        if (!state.renderableMesh) {
                            Errorf("Renderable %s mesh is null: %s",
                                ecs::ToString(lock, pair.first),
                                renderable.modelName);
                        }
                    }
                }
                if (renderable.textureOverrideName != state.renderableTextureOverrideName) {
                    state.renderableTextureOverrideName = renderable.textureOverrideName;
                    state.renderableTextureOverride = textures.LoadResource(renderable.textureOverrideName);
                }
            } else {
                state.renderableModelName = "";
                state.renderableMeshIndex = std::numeric_limits<MeshIndex>::max();
                state.renderableMesh.reset();
                state.renderableTextureOverrideName = "";
                state.renderableTextureOverride = {};
            }
            if (pair.first.Has<ecs::Light>(lock)) {
                auto &light = pair.first.Get<const ecs::Light>(lock);
                if (light.filterName != state.lightFilterName) {
                    state.lightFilterName = light.filterName;
                    state.lightFilter = textures.LoadResource(light.filterName);
                }
            } else {
                state.lightFilterName = "";
                state.lightFilter = {};
            }
        }
        for (const ecs::Entity &ent : entitiesToRemove) {
            stagingEntityState.erase(ent);
        }

        for (const ecs::Entity &ent : lock.EntitiesWith<ecs::Renderable>()) {
            if (!ent.Has<ecs::SceneInfo>(lock)) continue;
            if (ent.Get<ecs::SceneInfo>(lock).scene != scene) continue;
            auto &state = stagingEntityState[ent];
            auto &renderable = ent.Get<ecs::Renderable>(lock);
            if (renderable.modelName != state.renderableModelName ||
                renderable.meshIndex != state.renderableMeshIndex) {
                state.renderableModelName = renderable.modelName;
                state.renderableMeshIndex = renderable.meshIndex;
                if (!renderable.modelName.empty()) {
                    state.renderableMesh = LoadMesh(renderable);
                    if (!state.renderableMesh) {
                        Errorf("Renderable %s mesh is null: %s", ecs::ToString(lock, ent), renderable.modelName);
                    }
                }
            }
            if (state.renderableMesh) {
                if (!state.renderableMesh->Ready()) {
                    complete = false;
                } else {
                    auto vkMesh = state.renderableMesh->Get();
                    if (!vkMesh) {
                        Errorf("Renderable %s mesh is null: %s", ecs::ToString(lock, ent), renderable.modelName);
                        state.renderableMesh = nullptr;
                    } else if (!vkMesh->Valid()) {
                        if (!vkMesh->asset) {
                            Errorf("Renderable %s model is null: %s", ecs::ToString(lock, ent), renderable.modelName);
                        } else {
                            Errorf("Renderable %s mesh index is out of range: %u/%u",
                                ecs::ToString(lock, ent),
                                renderable.meshIndex,
                                vkMesh->asset->meshes.size());
                        }
                        state.renderableMesh = nullptr;
                        // Don't hang preloading if mesh is null
                    } else if (!vkMesh->CheckReady()) {
                        complete = false;
                    }
                }
            }
            const auto &textureOverrideName = renderable.textureOverrideName;
            if (!textureOverrideName.empty()) {
                if (state.renderableTextureOverrideName != textureOverrideName || !state.renderableTextureOverride) {
                    state.renderableTextureOverrideName = textureOverrideName;
                    state.renderableTextureOverride = textures.LoadResource(textureOverrideName);
                }
                if (!state.renderableTextureOverride.Ready()) complete = false;
            }
        }
        for (const ecs::Entity &ent : lock.EntitiesWith<ecs::Light>()) {
            if (!ent.Has<ecs::SceneInfo>(lock)) continue;
            if (ent.Get<ecs::SceneInfo>(lock).scene != scene) continue;
            auto &state = stagingEntityState[ent];
            const auto &filterName = ent.Get<ecs::Light>(lock).filterName;
            if (!filterName.empty()) {
                if (state.lightFilterName != filterName || !state.renderableTextureOverride) {
                    state.lightFilterName = filterName;
                    state.lightFilter = textures.LoadResource(filterName);
                }
                if (!state.lightFilter.Ready()) complete = false;
            }
        }
        return complete;
    }

    AsyncPtr<Mesh> GPUScene::LoadMesh(const ecs::Renderable &renderable) {
        if (renderable.modelName.empty()) return nullptr;
        ZoneScoped;
        ZoneStr(renderable.modelName.str());
        AsyncPtr<Mesh> asyncMesh = activeMeshes.Load(MeshKeyView{renderable.modelName, renderable.meshIndex});
        if (!asyncMesh) {
            AsyncPtr<Gltf> asyncModel = activeModels.Load(renderable.modelName);
            if (!asyncModel) {
                asyncModel = sp::Assets().LoadGltf(renderable.modelName);
                if (!asyncModel) return nullptr;
                activeModels.Register(renderable.modelName, asyncModel);
            }

            if (!asyncModel) return nullptr;
            asyncMesh = workQueue.Dispatch<Mesh>(NewDispatchSource,
                asyncModel,
                [this, meshIndex = renderable.meshIndex](std::shared_ptr<Gltf> model) {
                    if (!model || meshIndex >= model->meshes.size()) return std::shared_ptr<Mesh>();
                    return std::make_shared<Mesh>(model, meshIndex, *this, device);
                });
            activeMeshes.Register(MeshKey{rg::ResourceName{renderable.modelName}, renderable.meshIndex}, asyncMesh);
        }
        return asyncMesh;
    }

    void GPUScene::FlushMeshes() {
        ZoneScoped;
        workQueue.Flush(false, std::chrono::milliseconds(5));
        activeModels.Tick(std::chrono::milliseconds(33));
        activeMeshes.Tick(std::chrono::milliseconds(33));
    }

    GPUScene::DrawBufferIDs GPUScene::GenerateDrawsForView(rg::RenderGraph &graph,
        ecs::VisibilityMask viewMask,
        uint32_t instanceCount) {
        if (primitiveCount == 0 || vertexCount == 0 || instanceCount == 0) return GPUScene::DrawBufferIDs{};
        DrawBufferIDs bufferIDs;

        graph.AddPass("GenerateDrawsForView")
            .Build([&](rg::PassBuilder &builder) {
                const auto maxDraws = primitiveCountPowerOfTwo;

                graph.AddPass("Clear")
                    .Build([&](rg::PassBuilder &builder) {
                        auto drawCmds = builder.CreateBuffer(
                            {sizeof(uint32_t), sizeof(VkDrawIndexedIndirectCommand), maxDraws},
                            Residency::GPU_ONLY,
                            Access::TransferWrite);
                        bufferIDs.drawCommandsBuffer = drawCmds.id;
                    })
                    .Execute([bufferIDs](rg::Resources &resources, CommandContext &cmd) {
                        auto drawBuffer = resources.GetBuffer(bufferIDs.drawCommandsBuffer);
                        cmd.Raw().fillBuffer(*drawBuffer, 0, sizeof(uint32_t), 0);
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
                cmd.SetStorageBuffer("Renderables", "RenderableEntities");
                cmd.SetStorageBuffer("MeshModels", models);
                cmd.SetStorageBuffer("MeshPrimitives", primitiveLists);
                cmd.SetStorageBuffer("DrawCommands", bufferIDs.drawCommandsBuffer);
                cmd.SetStorageBuffer("DrawParamsList", bufferIDs.drawParamsBuffer);

                struct {
                    uint32_t renderableCount;
                    uint32_t instanceCount;
                    uint32_t visibilityMask;
                } constants;
                constants.renderableCount = gpuRenderables.size();
                constants.instanceCount = instanceCount;
                constants.visibilityMask = (uint32_t)viewMask;
                cmd.PushConstants(constants);

                cmd.Dispatch((gpuRenderables.size() + 127) / 128, 1, 1);
            });
        return bufferIDs;
    }

    GPUScene::DrawBufferIDs GPUScene::GenerateSortedDrawsForView(rg::RenderGraph &graph,
        glm::vec3 viewPosition,
        ecs::VisibilityMask viewMask,
        bool reverseSort,
        uint32_t instanceCount) {
        if (primitiveCount == 0 || vertexCount == 0 || instanceCount == 0) return GPUScene::DrawBufferIDs{};
        DrawBufferIDs bufferIDs;

        graph.AddPass("GenerateSortedDrawsForView")
            .Build([&](rg::PassBuilder &builder) {
                const auto maxDraws = primitiveCountPowerOfTwo;

                auto drawCmds = builder.CreateBuffer({sizeof(uint32_t), sizeof(VkDrawIndexedIndirectCommand), maxDraws},
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
                ZoneScopedN("GenerateSortedDrawsForView");
                static InlineVector<VkDrawIndexedIndirectCommand, 256 * 1024> drawCommands;
                static InlineVector<GPUDrawParams, 256 * 1024> drawParams;
                static InlineVector<float, 256 * 1024> primitiveDepth;
                drawCommands.clear();
                drawParams.clear();
                primitiveDepth.clear();

                for (size_t i = 0; i < gpuRenderables.size(); i++) {
                    auto &renderable = gpuRenderables[i];
                    if (((ecs::VisibilityMask)renderable.visibilityMask & viewMask) != viewMask) continue;

                    const ecs::Entity &ent = gpuRenderableEntities[i];
                    DebugAssertf(liveEntityState.count(ent) > 0,
                        "GPURenderableEntity %s has no state",
                        ecs::EntityRef(ent).Name().String());
                    DebugAssertf(liveEntityState[ent].renderableMesh,
                        "GPURenderableEntity %s has null mesh",
                        ecs::EntityRef(ent).Name().String());
                    auto mesh = liveEntityState[ent].renderableMesh->Get();
                    if (!mesh || !mesh->CheckReady()) continue;

                    for (auto &primitive : mesh->primitives) {
                        auto &drawCmd = drawCommands.emplace_back();

                        drawCmd.indexCount = primitive.indexCount;
                        drawCmd.instanceCount = instanceCount;
                        drawCmd.firstIndex = mesh->indexBuffer->ArrayOffset() + primitive.indexOffset;
                        drawCmd.vertexOffset = renderable.vertexOffset + primitive.vertexOffset;

                        drawCmd.firstInstance = drawParams.size();
                        auto &drawParam = drawParams.emplace_back();

                        drawParam.baseColorTexID = renderable.baseColorOverrideID >= 0 ? renderable.baseColorOverrideID
                                                                                       : primitive.baseColor.index;
                        drawParam.metallicRoughnessTexID = renderable.metallicRoughnessOverrideID >= 0
                                                               ? renderable.metallicRoughnessOverrideID
                                                               : primitive.metallicRoughness.index;
                        drawParam.opticID = renderable.opticID;
                        drawParam.emissiveScale = renderable.emissiveScale;

                        auto worldPos = renderable.modelToWorld * glm::vec4(primitive.center, 1);
                        auto relPos = (glm::vec3(worldPos) / worldPos.w) - viewPosition;
                        primitiveDepth.push_back(glm::length(relPos));
                    }
                }
                {
                    ZoneScopedN("SortedDrawCommands");
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
        if (primitiveCount == 0 || vertexCount == 0) return;

        cmd.SetBindlessDescriptors(2, textures.GetDescriptorSet());

        cmd.SetVertexLayout(SceneVertex::Layout());
        cmd.Raw().bindIndexBuffer(*indexBuffer, 0, vk::IndexType::eUint32);
        cmd.Raw().bindVertexBuffers(0, {*vertexBuffer}, {0});

        if (drawParamsBuffer) cmd.SetStorageBuffer(1, 0, drawParamsBuffer);
        cmd.DrawIndexedIndirectCount(drawCommandsBuffer,
            sizeof(uint32_t),
            drawCommandsBuffer,
            0,
            drawCommandsBuffer->ArraySize());
    }

    void GPUScene::AddGeometryWarp(rg::RenderGraph &graph) {
        if (primitiveCount == 0 || vertexCount == 0) return;

        graph.AddPass("GeometryWarpCalls")
            .Build([&](rg::PassBuilder &builder) {
                const auto maxDraws = primitiveCountPowerOfTwo;

                graph.AddPass("Clear")
                    .Build([&](rg::PassBuilder &builder) {
                        builder.CreateBuffer("WarpedVertexDrawCmds",
                            {sizeof(uint32_t), sizeof(VkDrawIndirectCommand), maxDraws},
                            Residency::GPU_ONLY,
                            Access::TransferWrite);
                    })
                    .Execute([](rg::Resources &resources, CommandContext &cmd) {
                        cmd.Raw().fillBuffer(*resources.GetBuffer("WarpedVertexDrawCmds"), 0, sizeof(uint32_t), 0);
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
                cmd.SetComputeShader("generate_warp_geometry_draws.comp");
                cmd.SetStorageBuffer("Renderables", "RenderableEntities");
                cmd.SetStorageBuffer("MeshModels", models);
                cmd.SetStorageBuffer("MeshPrimitives", primitiveLists);
                cmd.SetStorageBuffer("DrawCommands", "WarpedVertexDrawCmds");
                cmd.SetStorageBuffer("DrawParamsList", "WarpedVertexDrawParams");

                struct {
                    uint32_t renderableCount;
                } constants;
                constants.renderableCount = gpuRenderables.size();
                cmd.PushConstants(constants);
                cmd.Dispatch((gpuRenderables.size() + 127) / 128, 1, 1);
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
                cmd.SetStorageBuffer("DrawParamsList", paramBuffer);
                cmd.SetStorageBuffer("VertexBufferOutput", warpedVertexBuffer);
                cmd.SetUniformBuffer("JointPoses", "JointPoses");
                cmd.SetStorageBuffer("JointVertexData", jointsBuffer);

                cmd.SetVertexLayout(SceneVertex::Layout());
                cmd.SetPrimitiveTopology(vk::PrimitiveTopology::ePointList);
                cmd.Raw().bindVertexBuffers(0, {*vertexBuffer}, {0});
                cmd.DrawIndirect(cmdBuffer, sizeof(uint32_t), primitiveCount);
                cmd.EndRenderPass();
            });
    }

} // namespace sp::vulkan
