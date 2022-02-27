#include "Renderer.hh"

#include "assets/Gltf.hh"
#include "core/Logging.hh"
#include "core/Tracing.hh"
#include "ecs/EcsImpl.hh"
#include "game/SceneManager.hh"
#include "graphics/gui/MenuGuiManager.hh"
#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/core/Image.hh"
#include "graphics/vulkan/core/Util.hh"
#include "graphics/vulkan/gui/GuiRenderer.hh"
#include "graphics/vulkan/render_passes/VisualizeBuffer.hh"
#include "graphics/vulkan/scene/Mesh.hh"
#include "graphics/vulkan/scene/VertexLayouts.hh"

#ifdef SP_XR_SUPPORT
    #include "xr/XrSystem.hh"
#endif

namespace sp::vulkan {
    static const std::string defaultWindowViewTarget = "FlatView.LastOutput";
    static const std::string defaultXRViewTarget = "XRView.LastOutput";

    static CVar<float> CVarExposure("r.Exposure", 1.0f, "Scale factor for linear luminosity buffer");

    CVar<string> CVarWindowViewTarget("r.WindowViewTarget", defaultWindowViewTarget, "Primary window's render target");

    static CVar<uint32> CVarWindowViewTargetLayer("r.WindowViewTargetLayer", 0, "Array layer to view");

    static CVar<string> CVarXRViewTarget("r.XRViewTarget", defaultXRViewTarget, "HMD's render target");

    static CVar<float> CVarBloomScale("r.BloomScale", 0.15f, "Bloom scale");

    static CVar<bool> CVarSMAA("r.SMAA", true, "Enable SMAA");

    static CVar<bool> CVarVSM("r.VSM", false, "Enable Variance Shadow Mapping");

    static CVar<bool> CVarPCF("r.PCF", true, "Enable screen space shadow filtering");

    Renderer::Renderer(DeviceContext &device)
        : device(device), graph(device), scene(device), guiRenderer(new GuiRenderer(device)) {
        funcs.Register("listrendertargets", "List all render targets", [&]() {
            listRenderTargets = true;
        });

        auto lock = ecs::World.StartTransaction<ecs::AddRemove>();
        guiObserver = lock.Watch<ecs::ComponentEvent<ecs::Gui>>();
    }

    Renderer::~Renderer() {
        device->waitIdle();
    }

    void Renderer::RenderFrame() {
        pendingTransaction.test_and_set();
        BuildFrameGraph();
        pendingTransaction.clear();
        pendingTransaction.notify_all();

        if (listRenderTargets) {
            listRenderTargets = false;
            auto list = graph.AllRenderTargets();
            for (const auto &info : list) {
                auto &extent = info.desc.extent;
                Logf("%s (%dx%dx%d [%d] %s)",
                    info.name,
                    extent.width,
                    extent.height,
                    extent.depth,
                    info.desc.arrayLayers,
                    vk::to_string(info.desc.format));
            }
        }

        graph.Execute();
    }

    void Renderer::BuildFrameGraph() {
        ZoneScoped;
        auto lock = ecs::World.StartTransaction<ecs::Read<ecs::Name,
            ecs::TransformSnapshot,
            ecs::LaserLine,
            ecs::Light,
            ecs::Renderable,
            ecs::View,
            ecs::XRView,
            ecs::Mirror,
            ecs::Gui,
            ecs::Screen,
            ecs::FocusLock>>();

        AddSceneState(lock);
        AddLightState(lock);

        AddGeometryWarp();
        AddShadowMaps(lock);
        AddGuis(lock);

        {
            auto scope = graph.Scope("FlatView");
            AddFlatView(lock);
            if (graph.HasResource("GBuffer0")) {
                AddDeferredPasses(lock);
                if (lock.Get<ecs::FocusLock>().HasFocus(ecs::FocusLayer::MENU)) AddMenuOverlay();
            }
        }
#ifdef SP_XR_SUPPORT
        {
            auto scope = graph.Scope("XRView");
            AddXRView(lock);
            if (graph.HasResource("GBuffer0")) AddDeferredPasses(lock);
        }
        AddXRSubmit(lock);
#endif
        AddWindowOutput();
        screenshots.AddPass(graph);

        Assert(lock.UseCount() == 1, "something held onto the renderer lock");
    }

    void Renderer::AddWindowOutput() {
        auto swapchainImage = device.SwapchainImageView();
        if (!swapchainImage) return;

        rg::ResourceID sourceID = rg::InvalidResource;
        graph.AddPass("WindowFinalOutput")
            .Build([&](rg::PassBuilder &builder) {
                auto sourceName = CVarWindowViewTarget.Get();
                auto res = builder.GetResource(sourceName);
                if (!res && sourceName != defaultWindowViewTarget) {
                    Errorf("view target %s does not exist, defaulting to %s", sourceName, defaultWindowViewTarget);
                    CVarWindowViewTarget.Set(defaultWindowViewTarget);
                    res = builder.GetResource(defaultWindowViewTarget);
                }

                auto loadOp = LoadOp::DontCare;

                if (res) {
                    auto format = res.RenderTargetFormat();
                    auto layer = CVarWindowViewTargetLayer.Get();
                    if (FormatComponentCount(format) == 4 && FormatByteSize(format) == 4 && layer == 0) {
                        sourceID = res.id;
                    } else {
                        sourceID = renderer::VisualizeBuffer(graph, res.id, layer);
                    }
                    builder.ShaderRead(sourceID);
                } else {
                    loadOp = LoadOp::Clear;
                }

                RenderTargetDesc desc;
                desc.extent = swapchainImage->Extent();
                desc.format = swapchainImage->Format();
                builder.OutputColorAttachment(0, "WindowFinalOutput", desc, {loadOp, StoreOp::Store});
            })
            .Execute([this, sourceID](rg::Resources &resources, CommandContext &cmd) {
                if (sourceID != rg::InvalidResource) {
                    auto source = resources.GetRenderTarget(sourceID)->ImageView();
                    cmd.SetTexture(0, 0, source);
                    cmd.DrawScreenCover(source);
                }

                if (debugGui) guiRenderer->Render(*debugGui, cmd, vk::Rect2D{{0, 0}, cmd.GetFramebufferExtent()});
            });

        graph.SetTargetImageView("WindowFinalOutput", swapchainImage);
        graph.RequireResource("WindowFinalOutput");
    }

    void Renderer::AddFlatView(ecs::Lock<ecs::Read<ecs::TransformSnapshot, ecs::View>> lock) {
        ecs::Entity windowEntity = device.GetActiveView();

        if (!windowEntity || !windowEntity.Has<ecs::View>(lock)) return;

        auto view = windowEntity.Get<ecs::View>(lock);
        view.UpdateViewMatrix(lock, windowEntity);

        auto drawIDs = GenerateDrawsForView(view.visibilityMask);

        graph.AddPass("ForwardPass")
            .Build([&](rg::PassBuilder &builder) {
                RenderTargetDesc desc;
                desc.extent = vk::Extent3D(view.extents.x, view.extents.y, 1);
                desc.primaryViewType = vk::ImageViewType::e2DArray;

                desc.format = vk::Format::eR8G8B8A8Srgb;
                builder.OutputColorAttachment(0, "GBuffer0", desc, {LoadOp::Clear, StoreOp::Store});

                desc.format = vk::Format::eR16G16B16A16Sfloat;
                builder.OutputColorAttachment(1, "GBuffer1", desc, {LoadOp::Clear, StoreOp::Store});
                builder.OutputColorAttachment(2, "GBuffer2", desc, {LoadOp::Clear, StoreOp::Store});

                desc.format = vk::Format::eD24UnormS8Uint;
                builder.OutputDepthAttachment("GBufferDepthStencil", desc, {LoadOp::Clear, StoreOp::Store});

                builder.CreateUniformBuffer("ViewState", sizeof(GPUViewState) * 2);

                builder.ReadBuffer("WarpedVertexBuffer");
                builder.ReadBuffer(drawIDs.drawCommandsBuffer);
                builder.ReadBuffer(drawIDs.drawParamsBuffer);
            })
            .Execute([this, view, drawIDs](rg::Resources &resources, CommandContext &cmd) {
                cmd.SetShaders("scene.vert", "generate_gbuffer.frag");

                GPUViewState viewState[] = {{view}, {}};
                auto viewStateBuf = resources.GetBuffer("ViewState");
                viewStateBuf->CopyFrom(viewState, 2);
                cmd.SetUniformBuffer(0, 10, viewStateBuf);

                auto drawCommandsBuffer = resources.GetBuffer(drawIDs.drawCommandsBuffer);
                auto drawParamsBuffer = resources.GetBuffer(drawIDs.drawParamsBuffer);
                DrawSceneIndirect(cmd, resources, drawCommandsBuffer, drawParamsBuffer);
            });
    }

#ifdef SP_XR_SUPPORT
    void Renderer::AddXRView(ecs::Lock<ecs::Read<ecs::TransformSnapshot, ecs::View, ecs::XRView>> lock) {
        if (!xrSystem) return;

        auto xrViews = lock.EntitiesWith<ecs::XRView>();
        if (xrViews.size() == 0) return;

        glm::ivec2 viewExtents;
        std::array<ecs::View, (size_t)ecs::XrEye::Count> viewsByEye;

        for (auto &ent : xrViews) {
            if (!ent.Has<ecs::View>(lock)) continue;
            auto &view = ent.Get<ecs::View>(lock);

            if (viewExtents == glm::ivec2(0)) viewExtents = view.extents;
            Assert(viewExtents == view.extents, "All XR views must have the same extents");

            auto &xrView = ent.Get<ecs::XRView>(lock);
            viewsByEye[(size_t)xrView.eye] = view;
            viewsByEye[(size_t)xrView.eye].UpdateViewMatrix(lock, ent);
        }

        xrRenderPoses.resize(xrViews.size());

        if (!hiddenAreaMesh[0]) {
            for (size_t i = 0; i < hiddenAreaMesh.size(); i++) {
                auto mesh = xrSystem->GetHiddenAreaMesh(ecs::XrEye(i));
                if (mesh.triangleCount == 0) continue;
                hiddenAreaMesh[i] = device.CreateBuffer(mesh.vertices,
                    mesh.triangleCount * 3,
                    vk::BufferUsageFlagBits::eVertexBuffer,
                    VMA_MEMORY_USAGE_CPU_TO_GPU);
                hiddenAreaTriangleCount[i] = mesh.triangleCount;
            }
        }

        auto executeHiddenAreaStencil = [this](uint32 eyeIndex) {
            return [this, eyeIndex](rg::Resources &resources, CommandContext &cmd) {
                cmd.SetShaders("basic_ortho_stencil.vert", "noop.frag");

                glm::mat4 proj = MakeOrthographicProjection(0, 1, 1, 0);
                cmd.PushConstants(proj);

                cmd.SetCullMode(vk::CullModeFlagBits::eNone);
                cmd.SetDepthTest(false, false);
                cmd.SetStencilTest(true);
                cmd.SetStencilWriteMask(vk::StencilFaceFlagBits::eFrontAndBack, 1);
                cmd.SetStencilReference(vk::StencilFaceFlagBits::eFrontAndBack, 1);
                cmd.SetStencilCompareOp(vk::CompareOp::eAlways);
                cmd.SetStencilPassOp(vk::StencilOp::eReplace);
                cmd.SetStencilFailOp(vk::StencilOp::eReplace);
                cmd.SetStencilDepthFailOp(vk::StencilOp::eReplace);

                cmd.SetVertexLayout(PositionVertex2D::Layout());
                cmd.Raw().bindVertexBuffers(0, {*this->hiddenAreaMesh[eyeIndex]}, {0});
                cmd.Draw(this->hiddenAreaTriangleCount[eyeIndex] * 3);
            };
        };

        graph.AddPass("HiddenAreaStencil0")
            .Build([&](rg::PassBuilder &builder) {
                RenderTargetDesc desc;
                desc.extent = vk::Extent3D(viewExtents.x, viewExtents.y, 1);
                desc.arrayLayers = xrViews.size();
                desc.primaryViewType = vk::ImageViewType::e2DArray;
                desc.format = vk::Format::eD24UnormS8Uint;
                rg::AttachmentInfo attachment = {LoadOp::Clear, StoreOp::Store};
                attachment.arrayIndex = 0;
                builder.OutputDepthAttachment("GBufferDepthStencil", desc, attachment);
            })
            .Execute(executeHiddenAreaStencil(0));

        graph.AddPass("HiddenAreaStencil1")
            .Build([&](rg::PassBuilder &builder) {
                rg::AttachmentInfo attachment = {LoadOp::Clear, StoreOp::Store};
                attachment.arrayIndex = 1;
                builder.SetDepthAttachment("GBufferDepthStencil", attachment);
            })
            .Execute(executeHiddenAreaStencil(1));

        auto drawIDs = GenerateDrawsForView(viewsByEye[0].visibilityMask);

        graph.AddPass("ForwardPass")
            .Build([&](rg::PassBuilder &builder) {
                RenderTargetDesc desc;
                desc.extent = vk::Extent3D(viewExtents.x, viewExtents.y, 1);
                desc.arrayLayers = xrViews.size();
                desc.primaryViewType = vk::ImageViewType::e2DArray;

                desc.format = vk::Format::eR8G8B8A8Srgb;
                builder.OutputColorAttachment(0, "GBuffer0", desc, {LoadOp::Clear, StoreOp::Store});

                desc.format = vk::Format::eR16G16B16A16Sfloat;
                builder.OutputColorAttachment(1, "GBuffer1", desc, {LoadOp::Clear, StoreOp::Store});
                builder.OutputColorAttachment(2, "GBuffer2", desc, {LoadOp::Clear, StoreOp::Store});
                builder.SetDepthAttachment("GBufferDepthStencil", {LoadOp::Load, StoreOp::Store});

                builder.CreateUniformBuffer("ViewState", sizeof(GPUViewState) * 2);

                builder.ReadBuffer("WarpedVertexBuffer");
                builder.ReadBuffer(drawIDs.drawCommandsBuffer);
                builder.ReadBuffer(drawIDs.drawParamsBuffer);
            })
            .Execute([this, viewsByEye, drawIDs](rg::Resources &resources, CommandContext &cmd) {
                cmd.SetShaders("scene.vert", "generate_gbuffer.frag");

                cmd.SetStencilTest(true);
                cmd.SetStencilCompareOp(vk::CompareOp::eNotEqual);
                cmd.SetStencilCompareMask(vk::StencilFaceFlagBits::eFrontAndBack, 1);
                cmd.SetStencilReference(vk::StencilFaceFlagBits::eFrontAndBack, 1);

                auto viewStateBuf = resources.GetBuffer("ViewState");
                cmd.SetUniformBuffer(0, 10, viewStateBuf);

                auto drawCommandsBuffer = resources.GetBuffer(drawIDs.drawCommandsBuffer);
                auto drawParamsBuffer = resources.GetBuffer(drawIDs.drawParamsBuffer);
                DrawSceneIndirect(cmd, resources, drawCommandsBuffer, drawParamsBuffer);

                GPUViewState *viewState;
                viewStateBuf->Map((void **)&viewState);
                for (size_t i = 0; i < viewsByEye.size(); i++) {
                    auto view = viewsByEye[i];

                    if (this->xrSystem->GetPredictedViewPose(ecs::XrEye(i), this->xrRenderPoses[i])) {
                        view.SetViewMat(glm::inverse(this->xrRenderPoses[i]) * view.viewMat);
                    }

                    viewState[i] = GPUViewState(view);
                }
                viewStateBuf->Unmap();
                viewStateBuf->Flush();
            });
    }

    void Renderer::AddXRSubmit(ecs::Lock<ecs::Read<ecs::XRView>> lock) {
        if (!xrSystem) return;

        auto xrViews = lock.EntitiesWith<ecs::XRView>();
        if (xrViews.size() != 2) return;

        rg::ResourceID sourceID;
        graph.AddPass("XRSubmit")
            .Build([&](rg::PassBuilder &builder) {
                auto res = builder.GetResource(CVarXRViewTarget.Get());
                if (!res) {
                    Errorf("view target %s does not exist, defaulting to %s",
                        CVarXRViewTarget.Get(),
                        defaultXRViewTarget);
                    CVarXRViewTarget.Set(defaultXRViewTarget);
                    res = builder.GetResource(defaultXRViewTarget);
                }

                auto format = res.RenderTargetFormat();
                if (FormatComponentCount(format) == 4 && FormatByteSize(format) == 4) {
                    sourceID = res.id;
                } else {
                    sourceID = renderer::VisualizeBuffer(graph, res.id);
                }

                builder.TransferRead(sourceID);
                builder.RequirePass();
            })
            .Execute([this, sourceID](rg::Resources &resources, DeviceContext &device) {
                auto xrRenderTarget = resources.GetRenderTarget(sourceID);

                for (size_t i = 0; i < 2; i++) {
                    this->xrSystem->SubmitView(ecs::XrEye(i),
                        this->xrRenderPoses[i],
                        xrRenderTarget->ImageView().get());
                }
            });
    }
#endif

    void Renderer::AddSceneState(ecs::Lock<ecs::Read<ecs::Renderable, ecs::TransformSnapshot>> lock) {
        if (scene.textures.Count() == 0) scene.textures.Add(GetBlankPixelImage());

        scene.renderableCount = 0;
        scene.primitiveCount = 0;
        scene.vertexCount = 0;

        scene.renderableEntityList = device.GetFramePooledBuffer(BUFFER_TYPE_STORAGE_TRANSFER, 1024 * 1024);

        auto gpuRenderable = (GPURenderableEntity *)scene.renderableEntityList->Mapped();
        bool hasPendingModel = false;

        for (auto &ent : lock.EntitiesWith<ecs::Renderable>()) {
            if (!ent.Has<ecs::TransformSnapshot>(lock)) continue;

            auto &renderable = ent.Get<ecs::Renderable>(lock);
            if (!renderable.model || !renderable.model->Ready()) continue;

            auto model = renderable.model->Get();
            if (!model) continue;
            auto meshName = model->name + "." + std::to_string(renderable.meshIndex);
            auto vkMesh = activeMeshes.Load(meshName);
            if (!vkMesh) {
                hasPendingModel = true;
                meshesToLoad.emplace_back(model, renderable.meshIndex);
                continue;
            }
            if (!vkMesh->CheckReady()) {
                hasPendingModel = true;
                continue;
            }

            Assert(scene.renderableCount * sizeof(GPURenderableEntity) < scene.renderableEntityList->Size(),
                "renderable entity overflow");

            gpuRenderable->modelToWorld = ent.Get<ecs::TransformSnapshot>(lock).matrix;
            gpuRenderable->visibilityMask = renderable.visibility.to_ulong();
            gpuRenderable->meshIndex = vkMesh->SceneIndex();
            gpuRenderable->vertexOffset = scene.vertexCount;
            gpuRenderable++;
            scene.renderableCount++;
            scene.primitiveCount += vkMesh->PrimitiveCount();
            scene.vertexCount += vkMesh->VertexCount();
        }

        scene.primitiveCountPowerOfTwo = std::max(1u, CeilToPowerOfTwo(scene.primitiveCount));

        if (!hasPendingModel) {
            sceneReady.test_and_set();
            sceneReady.notify_all();
        } else {
            sceneReady.clear();
        }
    }

    void Renderer::AddLightState(ecs::Lock<ecs::Read<ecs::Light, ecs::TransformSnapshot>> lock) {
        int lightCount = 0;
        int gelCount = 0;
        glm::ivec2 renderTargetSize(0, 0);

        for (auto entity : lock.EntitiesWith<ecs::Light>()) {
            if (!entity.Has<ecs::TransformSnapshot>(lock)) continue;

            auto &light = entity.Get<ecs::Light>(lock);
            if (!light.on) continue;

            int extent = (int)std::pow(2, light.shadowMapSize);

            auto &transform = entity.Get<ecs::TransformSnapshot>(lock);
            auto &view = lights.views[lightCount];

            view.visibilityMask.set(ecs::Renderable::VISIBLE_LIGHTING_SHADOW);
            view.extents = {extent, extent};
            view.fov = light.spotAngle * 2.0f;
            view.offset = {renderTargetSize.x, 0};
            view.clearMode.reset();
            view.clip = light.shadowMapClip;
            view.UpdateProjectionMatrix();
            view.UpdateViewMatrix(lock, entity);

            auto &data = lights.gpuData.lights[lightCount];
            data.position = transform.GetPosition();
            data.tint = light.tint;
            data.direction = transform.GetForward();
            data.spotAngleCos = cos(light.spotAngle);
            data.proj = view.projMat;
            data.invProj = view.invProjMat;
            data.view = view.viewMat;
            data.clip = view.clip;
            data.mapOffset = {renderTargetSize.x, 0, extent, extent};
            data.intensity = light.intensity;
            data.illuminance = light.illuminance;

            if (light.gelName.empty()) {
                data.gelId = 0;
            } else {
                Assert(gelCount < MAX_LIGHT_GELS, "too many light gels");
                data.gelId = ++gelCount;
                lights.gelNames[data.gelId - 1] = light.gelName;
            }

            renderTargetSize.x += extent;
            if (extent > renderTargetSize.y) renderTargetSize.y = extent;
            if (++lightCount >= MAX_LIGHTS) break;
        }
        glm::vec4 mapOffsetScale(renderTargetSize, renderTargetSize);
        for (int i = 0; i < lightCount; i++) {
            lights.gpuData.lights[i].mapOffset /= mapOffsetScale;
        }
        lights.renderTargetSize = renderTargetSize;
        lights.count = lightCount;
        lights.gelCount = gelCount;
        lights.gpuData.count = lightCount;

        graph.AddPass("LightState")
            .Build([&](rg::PassBuilder &builder) {
                builder.CreateUniformBuffer("LightState", sizeof(lights.gpuData));
            })
            .Execute([this](rg::Resources &resources, DeviceContext &) {
                resources.GetBuffer("LightState")->CopyFrom(&lights.gpuData);
            });
    }

    void Renderer::AddGeometryWarp() {
        graph.AddPass("GeometryWarp")
            .Build([&](rg::PassBuilder &builder) {
                const auto maxDraws = scene.primitiveCountPowerOfTwo;

                builder.CreateBuffer(BUFFER_TYPE_STORAGE_LOCAL_INDIRECT,
                    "WarpedVertexDrawCmds",
                    sizeof(uint32) + maxDraws * sizeof(VkDrawIndirectCommand));

                builder.CreateBuffer(BUFFER_TYPE_STORAGE_LOCAL,
                    "WarpedVertexDrawParams",
                    maxDraws * sizeof(glm::vec4) * 5);

                builder.CreateBuffer(BUFFER_TYPE_STORAGE_LOCAL_VERTEX,
                    "WarpedVertexBuffer",
                    sizeof(SceneVertex) * scene.vertexCount);
            })
            .Execute([this](rg::Resources &resources, CommandContext &cmd) {
                if (scene.vertexCount == 0) return;

                vk::BufferMemoryBarrier barrier;
                barrier.srcAccessMask = vk::AccessFlagBits::eHostWrite;
                barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
                barrier.buffer = *scene.renderableEntityList;
                barrier.size = VK_WHOLE_SIZE;
                cmd.Raw().pipelineBarrier(vk::PipelineStageFlagBits::eHost,
                    vk::PipelineStageFlagBits::eComputeShader,
                    {},
                    {},
                    {barrier},
                    {});

                auto cmdBuffer = resources.GetBuffer("WarpedVertexDrawCmds");
                auto paramBuffer = resources.GetBuffer("WarpedVertexDrawParams");
                auto vertexBuffer = resources.GetBuffer("WarpedVertexBuffer");

                cmd.Raw().fillBuffer(*cmdBuffer, 0, sizeof(uint32), 0);

                cmd.SetComputeShader("generate_warp_geometry_draws.comp");
                cmd.SetStorageBuffer(0, 0, scene.renderableEntityList);
                cmd.SetStorageBuffer(0, 1, scene.models);
                cmd.SetStorageBuffer(0, 2, scene.primitiveLists);
                cmd.SetStorageBuffer(0, 3, cmdBuffer);
                cmd.SetStorageBuffer(0, 4, paramBuffer);

                struct {
                    uint32 renderableCount;
                } constants;
                constants.renderableCount = scene.renderableCount;
                cmd.PushConstants(constants);
                cmd.Dispatch((scene.renderableCount + 127) / 128, 1, 1);

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
                cmd.SetStorageBuffer(0, 1, vertexBuffer);
                cmd.SetVertexLayout(SceneVertex::Layout());
                cmd.SetPrimitiveTopology(vk::PrimitiveTopology::ePointList);
                cmd.Raw().bindVertexBuffers(0, {*scene.vertexBuffer}, {0});
                cmd.DrawIndirect(cmdBuffer, sizeof(uint32), scene.primitiveCount);
                cmd.EndRenderPass();
            });
    }

    Renderer::DrawBufferIDs Renderer::GenerateDrawsForView(ecs::Renderable::VisibilityMask viewMask) {
        DrawBufferIDs bufferIDs;

        graph.AddPass("GenerateDrawsForView")
            .Build([&](rg::PassBuilder &builder) {
                const auto maxDraws = scene.primitiveCountPowerOfTwo;

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
                cmd.SetStorageBuffer(0, 0, scene.renderableEntityList);
                cmd.SetStorageBuffer(0, 1, scene.models);
                cmd.SetStorageBuffer(0, 2, scene.primitiveLists);
                cmd.SetStorageBuffer(0, 3, drawBuffer);
                cmd.SetStorageBuffer(0, 4, resources.GetBuffer(bufferIDs.drawParamsBuffer));

                struct {
                    uint32 renderableCount;
                    uint32 visibilityMask;
                } constants;
                constants.renderableCount = scene.renderableCount;
                constants.visibilityMask = viewMask.to_ulong();
                cmd.PushConstants(constants);

                cmd.Dispatch((scene.renderableCount + 127) / 128, 1, 1);
            });
        return bufferIDs;
    }

    void Renderer::DrawSceneIndirect(CommandContext &cmd,
        rg::Resources &resources,
        BufferPtr drawCommandsBuffer,
        BufferPtr drawParamsBuffer) {
        if (scene.vertexCount == 0) return;

        cmd.SetBindlessDescriptors(2, scene.textures.GetDescriptorSet());

        cmd.SetVertexLayout(SceneVertex::Layout());
        cmd.Raw().bindIndexBuffer(*scene.indexBuffer, 0, vk::IndexType::eUint32);
        cmd.Raw().bindVertexBuffers(0, {*resources.GetBuffer("WarpedVertexBuffer")}, {0});

        if (drawParamsBuffer) cmd.SetStorageBuffer(1, 0, drawParamsBuffer);
        cmd.DrawIndexedIndirectCount(drawCommandsBuffer,
            sizeof(uint32),
            drawCommandsBuffer,
            0,
            drawCommandsBuffer->Size() / sizeof(VkDrawIndexedIndirectCommand));
    }

    void Renderer::AddShadowMaps(DrawLock lock) {
        vector<DrawBufferIDs> drawIDs;
        drawIDs.reserve(lights.count);
        for (int i = 0; i < lights.count; i++) {
            drawIDs.push_back(GenerateDrawsForView(lights.views[i].visibilityMask));
        }

        graph.AddPass("ShadowMaps")
            .Build([&](rg::PassBuilder &builder) {
                RenderTargetDesc desc;
                auto extent = glm::max(glm::ivec2(1), lights.renderTargetSize);
                desc.extent = vk::Extent3D(extent.x, extent.y, 1);

                desc.format = CVarVSM.Get() ? vk::Format::eR32G32Sfloat : vk::Format::eR32Sfloat;
                builder.OutputColorAttachment(0, "ShadowMapLinear", desc, {LoadOp::Clear, StoreOp::Store});

                desc.format = vk::Format::eD16Unorm;
                builder.OutputDepthAttachment("ShadowMapDepth", desc, {LoadOp::Clear, StoreOp::Store});

                builder.ReadBuffer("WarpedVertexBuffer");

                for (auto &ids : drawIDs) {
                    builder.ReadBuffer(ids.drawCommandsBuffer);
                }
            })

            .Execute([this, drawIDs](rg::Resources &resources, CommandContext &cmd) {
                cmd.SetShaders("shadow_map.vert", CVarVSM.Get() ? "shadow_map_vsm.frag" : "shadow_map.frag");

                auto &lights = this->lights;
                for (int i = 0; i < lights.count; i++) {
                    auto &view = lights.views[i];

                    GPUViewState views[] = {{view}, {}};
                    cmd.UploadUniformData(0, 10, views, 2);

                    vk::Rect2D viewport;
                    viewport.extent = vk::Extent2D(view.extents.x, view.extents.y);
                    viewport.offset = vk::Offset2D(view.offset.x, view.offset.y);
                    cmd.SetViewport(viewport);
                    cmd.SetYDirection(YDirection::Down);

                    auto &ids = drawIDs[i];
                    DrawSceneIndirect(cmd, resources, resources.GetBuffer(ids.drawCommandsBuffer), {});
                }
            });

        graph.BeginScope("ShadowMapBlur");
        auto sourceID = graph.LastOutputID();
        auto blurY1 = AddGaussianBlur(sourceID, glm::ivec2(0, 1), 1);
        AddGaussianBlur(blurY1, glm::ivec2(1, 0), 1);
        graph.EndScope();
    }

    void Renderer::AddGuis(ecs::Lock<ecs::Read<ecs::Gui>> lock) {
        ecs::ComponentEvent<ecs::Gui> guiEvent;
        while (guiObserver.Poll(lock, guiEvent)) {
            auto &eventEntity = guiEvent.entity;

            if (guiEvent.type == Tecs::EventType::REMOVED) {
                for (auto it = guis.begin(); it != guis.end(); it++) {
                    if (it->entity == eventEntity) {
                        guis.erase(it);
                        break;
                    }
                }
            } else if (guiEvent.type == Tecs::EventType::ADDED) {
                const auto &guiComponent = eventEntity.Get<ecs::Gui>(lock);
                guis.emplace_back(RenderableGui{guiEvent.entity, guiComponent.manager});
            }
        }

        for (auto &gui : guis) {
            graph.AddPass("Gui")
                .Build([&](rg::PassBuilder &builder) {
                    RenderTargetDesc desc;
                    desc.format = vk::Format::eR8G8B8A8Srgb;

                    // TODO: figure out a good size based on the transform of the gui
                    desc.extent = vk::Extent3D(1024, 1024, 1);
                    MenuGuiManager *manager = dynamic_cast<MenuGuiManager *>(gui.manager);
                    if (manager && manager->RenderMode() == MenuRenderMode::Pause) {
                        desc.extent = vk::Extent3D(1920, 1080, 1);
                    }

                    const auto &name = gui.manager->Name();
                    auto target = builder.OutputColorAttachment(0, name, desc, {LoadOp::Clear, StoreOp::Store});
                    gui.renderGraphID = target.id;
                })
                .Execute([this, gui](rg::Resources &resources, CommandContext &cmd) {
                    auto &extent = resources.GetRenderTarget(gui.renderGraphID)->Desc().extent;
                    vk::Rect2D viewport = {{}, {extent.width, extent.height}};
                    guiRenderer->Render(*gui.manager, cmd, viewport);
                });
        }
    }

    void Renderer::AddDeferredPasses(ecs::Lock<ecs::Read<ecs::TransformSnapshot, ecs::Screen, ecs::LaserLine>> lock) {
        AddLighting();
        emissive.AddPass(graph, lock);
        AddBloom();
        AddTonemap();

        if (CVarSMAA.Get()) smaa.AddPass(graph);
    }

    void Renderer::AddLighting() {
        auto depthTarget = CVarVSM.Get() ? "ShadowMapBlur.LastOutput" : "ShadowMapLinear";

        graph.AddPass("Lighting")
            .Build([&](rg::PassBuilder &builder) {
                auto gBuffer0 = builder.ShaderRead("GBuffer0");
                builder.ShaderRead("GBuffer1");
                builder.ShaderRead("GBuffer2");
                builder.ShaderRead(depthTarget);

                auto desc = gBuffer0.DeriveRenderTarget();
                desc.format = vk::Format::eR16G16B16A16Sfloat;
                builder.OutputColorAttachment(0, "LinearLuminance", desc, {LoadOp::DontCare, StoreOp::Store});

                builder.ReadBuffer("ViewState");
                builder.ReadBuffer("LightState");

                for (int i = 0; i < this->lights.gelCount; i++) {
                    builder.ShaderRead(this->lights.gelNames[i]);
                }

                builder.SetDepthAttachment("GBufferDepthStencil", {LoadOp::Load, StoreOp::Store});
            })
            .Execute([this, depthTarget](rg::Resources &resources, CommandContext &cmd) {
                auto frag = CVarVSM.Get() ? "lighting_vsm.frag" : CVarPCF.Get() ? "lighting_pcf.frag" : "lighting.frag";
                cmd.SetShaders("screen_cover.vert", frag);
                cmd.SetStencilTest(true);
                cmd.SetDepthTest(false, false);
                cmd.SetStencilCompareOp(vk::CompareOp::eNotEqual);
                cmd.SetStencilCompareMask(vk::StencilFaceFlagBits::eFrontAndBack, 1);
                cmd.SetStencilReference(vk::StencilFaceFlagBits::eFrontAndBack, 1);

                cmd.SetTexture(0, 0, resources.GetRenderTarget("GBuffer0")->ImageView());
                cmd.SetTexture(0, 1, resources.GetRenderTarget("GBuffer1")->ImageView());
                cmd.SetTexture(0, 2, resources.GetRenderTarget("GBuffer2")->ImageView());
                cmd.SetTexture(0, 3, resources.GetRenderTarget(depthTarget)->ImageView());

                cmd.PushConstants(CVarExposure.Get());

                for (int i = 0; i < MAX_LIGHT_GELS; i++) {
                    if (i < this->lights.gelCount) {
                        const auto &target = resources.GetRenderTarget(this->lights.gelNames[i]);
                        cmd.SetTexture(1, i, target->ImageView());
                    } else {
                        cmd.SetTexture(1, i, this->GetBlankPixelImage());
                    }
                }

                cmd.SetUniformBuffer(0, 10, resources.GetBuffer("ViewState"));
                cmd.SetUniformBuffer(0, 11, resources.GetBuffer("LightState"));
                cmd.Draw(3);
            });
    }

    void Renderer::AddTonemap() {
        graph.AddPass("Tonemap")
            .Build([&](rg::PassBuilder &builder) {
                auto luminance = builder.ShaderRead(builder.LastOutputID());

                auto desc = luminance.DeriveRenderTarget();
                desc.format = vk::Format::eR8G8B8A8Srgb;
                builder.OutputColorAttachment(0, "TonemappedLuminance", desc, {LoadOp::DontCare, StoreOp::Store});
            })
            .Execute([](rg::Resources &resources, CommandContext &cmd) {
                cmd.SetShaders("screen_cover.vert", "tonemap.frag");
                cmd.SetTexture(0, 0, resources.GetRenderTarget(resources.LastOutputID())->ImageView());
                cmd.Draw(3);
            });
    }

    rg::ResourceID Renderer::AddGaussianBlur(rg::ResourceID sourceID,
        glm::ivec2 direction,
        uint32 downsample,
        float scale,
        float clip) {

        struct {
            glm::vec2 direction;
            float threshold;
            float scale;
        } constants;

        constants.direction = glm::vec2(direction);
        constants.threshold = clip;
        constants.scale = scale;

        rg::ResourceID destID;
        graph.AddPass("GaussianBlur")
            .Build([&](rg::PassBuilder &builder) {
                auto source = builder.ShaderRead(sourceID);
                auto desc = source.DeriveRenderTarget();
                desc.extent.width = std::max(desc.extent.width / downsample, 1u);
                desc.extent.height = std::max(desc.extent.height / downsample, 1u);
                auto dest = builder.OutputColorAttachment(0, "", desc, {LoadOp::DontCare, StoreOp::Store});
                destID = dest.id;
            })
            .Execute([sourceID, constants](rg::Resources &resources, CommandContext &cmd) {
                auto source = resources.GetRenderTarget(sourceID);

                if (source->Desc().primaryViewType == vk::ImageViewType::e2DArray) {
                    cmd.SetShaders("screen_cover.vert", "gaussian_blur_array.frag");
                } else {
                    cmd.SetShaders("screen_cover.vert", "gaussian_blur.frag");
                }

                cmd.SetTexture(0, 0, source->ImageView());
                cmd.PushConstants(constants);
                cmd.Draw(3);
            });
        return destID;
    }

    rg::ResourceID Renderer::AddBloom() {
        graph.BeginScope("BloomBlur");
        auto sourceID = graph.LastOutputID();
        auto blurY1 = AddGaussianBlur(sourceID, glm::ivec2(0, 1), 1, CVarBloomScale.Get());
        auto blurX1 = AddGaussianBlur(blurY1, glm::ivec2(1, 0), 2);
        auto blurY2 = AddGaussianBlur(blurX1, glm::ivec2(0, 1), 1);
        auto blurX2 = AddGaussianBlur(blurY2, glm::ivec2(1, 0), 1);
        graph.EndScope();

        rg::ResourceID destID;
        graph.AddPass("BloomCombine")
            .Build([&](rg::PassBuilder &builder) {
                auto source = builder.ShaderRead(sourceID);
                auto desc = source.DeriveRenderTarget();
                auto dest = builder.OutputColorAttachment(0, "Bloom", desc, {LoadOp::DontCare, StoreOp::Store});
                destID = dest.id;

                builder.ShaderRead(blurX1);
                builder.ShaderRead(blurX2);
            })
            .Execute([sourceID, blurX1, blurX2](rg::Resources &resources, CommandContext &cmd) {
                cmd.SetShaders("screen_cover.vert", "bloom_combine.frag");
                cmd.SetTexture(0, 0, resources.GetRenderTarget(sourceID)->ImageView());
                cmd.SetTexture(0, 1, resources.GetRenderTarget(blurX1)->ImageView());
                cmd.SetTexture(0, 2, resources.GetRenderTarget(blurX2)->ImageView());
                cmd.Draw(3);
            });
        return destID;
    }

    void Renderer::AddMenuOverlay() {
        rg::ResourceID menuID = rg::InvalidResource;
        for (auto &gui : guis) {
            if (gui.manager->Name() == "menu_gui") {
                menuID = gui.renderGraphID;
                MenuGuiManager *manager = dynamic_cast<MenuGuiManager *>(gui.manager);
                if (!manager || manager->RenderMode() != MenuRenderMode::Pause) return;
                break;
            }
        }
        Assert(menuID != rg::InvalidResource, "main menu doesn't exist");

        graph.AddPass("MenuOverlay")
            .Build([&](rg::PassBuilder &builder) {
                auto input = builder.LastOutput();
                builder.ShaderRead(input.id);

                auto desc = input.DeriveRenderTarget();
                builder.OutputColorAttachment(0, "Menu", desc, {LoadOp::DontCare, StoreOp::Store});

                builder.ShaderRead(menuID);
            })
            .Execute([menuID](rg::Resources &resources, CommandContext &cmd) {
                cmd.DrawScreenCover(resources.GetRenderTarget(resources.LastOutputID())->ImageView());
                cmd.DrawScreenCover(resources.GetRenderTarget(menuID)->ImageView());
            });
    }

    void Renderer::EndFrame() {
        ZoneScoped;
        guiRenderer->Tick();
        activeMeshes.Tick(std::chrono::milliseconds(33)); // Minimum 30 fps tick rate

        GetSceneManager().PreloadSceneGraphics([this](auto lock, auto scene) {
            bool complete = true;
            for (auto ent : lock.template EntitiesWith<ecs::Renderable>()) {
                if (!ent.template Has<ecs::SceneInfo>(lock)) continue;
                if (ent.template Get<ecs::SceneInfo>(lock).scene.lock() != scene) continue;

                auto &renderable = ent.template Get<ecs::Renderable>(lock);
                if (!renderable.model) continue;
                if (!renderable.model->Ready()) {
                    complete = false;
                    continue;
                }

                auto model = renderable.model->Get();
                if (!model) {
                    Errorf("Preloading renderable with null model: %s", ecs::ToString(lock, ent));
                    continue;
                }
                auto meshName = model->name + "." + std::to_string(renderable.meshIndex);
                auto vkMesh = activeMeshes.Load(meshName);
                if (!vkMesh) {
                    complete = false;
                    meshesToLoad.emplace_back(model, renderable.meshIndex);
                    continue;
                }
                if (!vkMesh->CheckReady()) complete = false;
            }
            return complete;
        });

        for (int i = (int)meshesToLoad.size() - 1; i >= 0; i--) {
            auto &[model, meshIndex] = meshesToLoad[i];
            auto meshName = model->name + "." + std::to_string(meshIndex);
            if (activeMeshes.Contains(meshName)) {
                meshesToLoad.pop_back();
                continue;
            }

            activeMeshes.Register(meshName, make_shared<Mesh>(model, meshIndex, scene, device));
            meshesToLoad.pop_back();
        }

        scene.Flush();
    }

    void Renderer::SetDebugGui(GuiManager &gui) {
        debugGui = &gui;
    }

    ImageViewPtr Renderer::GetBlankPixelImage() {
        if (!blankPixelImage) blankPixelImage = CreateSinglePixelImage(glm::vec4(1));
        return blankPixelImage;
    }

    ImageViewPtr Renderer::CreateSinglePixelImage(glm::vec4 value) {
        uint8 data[4];
        for (size_t i = 0; i < 4; i++) {
            data[i] = (uint8_t)(255.0 * value[i]);
        }

        ImageCreateInfo imageInfo;
        imageInfo.imageType = vk::ImageType::e2D;
        imageInfo.usage = vk::ImageUsageFlagBits::eSampled;
        imageInfo.format = vk::Format::eR8G8B8A8Unorm;
        imageInfo.extent = vk::Extent3D(1, 1, 1);

        ImageViewCreateInfo viewInfo;
        viewInfo.defaultSampler = device.GetSampler(SamplerType::NearestTiled);
        auto fut = device.CreateImageAndView(imageInfo, viewInfo, {data, sizeof(data)});
        device.FlushMainQueue();
        return fut->Get();
    }
} // namespace sp::vulkan
