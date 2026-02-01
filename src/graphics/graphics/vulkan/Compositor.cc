/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Compositor.hh"

#include "assets/Asset.hh"
#include "assets/AssetManager.hh"
#include "common/Async.hh"
#include "common/Common.hh"
#include "common/Defer.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/ScriptGuiDefinition.hh"
#include "ecs/ScriptManager.hh"
#include "ecs/SignalRef.hh"
#include "graphics/core/GraphicsContext.hh"
#include "graphics/vulkan/Renderer.hh"
#include "graphics/vulkan/core/Access.hh"
#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/core/Util.hh"
#include "graphics/vulkan/core/VkCommon.hh"
#include "graphics/vulkan/render_graph/PooledImage.hh"
#include "graphics/vulkan/render_graph/RenderGraph.hh"
#include "graphics/vulkan/render_graph/Resources.hh"
#include "graphics/vulkan/render_passes/Blur.hh"
#include "graphics/vulkan/render_passes/Mipmap.hh"
#include "gui/GuiContext.hh"
#include "gui/ImGuiHelpers.hh"
#include "vulkan/vulkan.hpp"

#include <algorithm>
#include <cstdint>
#include <imgui.h>
#include <limits>
#include <memory>
#include <mutex>
#include <span>

namespace sp::vulkan {
    Compositor::Compositor(DeviceContext &device, rg::RenderGraph &graph) : device(device), graph(graph) {
        vertexLayout = make_unique<VertexLayout>(0, sizeof(GuiDrawVertex));
        vertexLayout->PushAttribute(0, 0, vk::Format::eR32G32Sfloat, offsetof(GuiDrawVertex, pos));
        vertexLayout->PushAttribute(1, 0, vk::Format::eR32G32Sfloat, offsetof(GuiDrawVertex, uv));
        vertexLayout->PushAttribute(2, 0, vk::Format::eR8G8B8A8Unorm, offsetof(GuiDrawVertex, col));

        fontAtlas = make_shared<ImFontAtlas>();

        fontAtlas->AddFontDefault(nullptr);

        static const ImWchar glyphRanges[] = {
            0x0020,
            0x00FF, // Basic Latin + Latin Supplement
            0x2100,
            0x214F, // Letterlike Symbols
            0,
        };

        for (auto &def : GetGuiFontList()) {
            auto asset = Assets().Load("fonts/"s + def.name)->Get();
            Assertf(asset, "Failed to load gui font %s", def.name);

            ImFontConfig cfg = {};
            cfg.FontData = (void *)asset->BufferPtr();
            cfg.FontDataSize = asset->BufferSize();
            cfg.FontDataOwnedByAtlas = false;
            cfg.SizePixels = def.size;
            cfg.GlyphRanges = &glyphRanges[0];
            auto filename = asset->path.filename().string();
            strncpy(cfg.Name, filename.c_str(), std::min(sizeof(cfg.Name) - 1, filename.length()));
            fontAtlas->AddFont(&cfg);
        }

        uint8 *fontData;
        int fontWidth, fontHeight;
        fontAtlas->GetTexDataAsRGBA32(&fontData, &fontWidth, &fontHeight);

        ImageCreateInfo fontImageInfo;
        fontImageInfo.imageType = vk::ImageType::e2D;
        fontImageInfo.extent = vk::Extent3D{(uint32)fontWidth, (uint32)fontHeight, 1};
        fontImageInfo.format = vk::Format::eR8G8B8A8Unorm;
        fontImageInfo.usage = vk::ImageUsageFlagBits::eSampled;

        ImageViewCreateInfo fontViewInfo = {};
        fontViewInfo.defaultSampler = device.GetSampler(SamplerType::BilinearClampEdge);

        fontView = device.CreateImageAndView(fontImageInfo,
            fontViewInfo,
            InitialData(fontData, size_t(fontWidth * fontHeight * 4)));

        EndFrame();
    }

    void Compositor::DrawGuiContext(GuiContext &context, glm::ivec4 viewport, glm::vec2 scale) {
        vk::Rect2D viewportRect({viewport.x, viewport.y}, {(uint32_t)viewport.z, (uint32_t)viewport.w});
        ImGui::SetCurrentContext(nullptr); // Don't leak contexts between instances
        if (context.SetGuiContext()) {
            ImGui::GetMainViewport()->PlatformHandleRaw = device.Win32WindowHandle();

            ImGuiIO &io = ImGui::GetIO();
            io.IniFilename = nullptr;
            io.DisplaySize = ImVec2(viewport.z / scale.x, viewport.w / scale.y);
            io.DisplayFramebufferScale = ImVec2(scale.x, scale.y);
            io.DeltaTime = deltaTime;
            io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

            auto lastFonts = io.Fonts;
            io.Fonts = fontAtlas.get();
            io.Fonts->TexID = FontAtlasID;
            Defer defer([&] {
                io.Fonts = lastFonts;
            });

            ImGui::NewFrame();
            context.DefineWindows();
            ImGui::Render();

            ImDrawData *drawData = ImGui::GetDrawData();
            drawData->ScaleClipRects(io.DisplayFramebufferScale);
            internalDrawGui(drawData, viewportRect, scale, true);
        } else {
            GuiDrawData drawData = {};
            context.GetDrawData(drawData);
            internalDrawGui(drawData, viewportRect, scale);
        }
    }

    void Compositor::DrawGuiData(const GuiDrawData &drawData, glm::ivec4 viewport, glm::vec2 scale) {
        if (drawData.drawCommands.empty()) return;

        Assertf(viewport.z > 0 && viewport.w > 0,
            "Compositor::DrawGuiData called with invalid viewport: %s",
            glm::to_string(viewport));
        vk::Rect2D viewportRect({viewport.x, viewport.y}, {(uint32_t)viewport.z, (uint32_t)viewport.w});
        internalDrawGui(drawData, viewportRect, scale);
    }

    std::shared_ptr<GpuTexture> Compositor::UploadStaticImage(std::shared_ptr<const sp::Image> image,
        bool genMipmap,
        bool srgb) {
        ZoneScoped;
        Assertf(image, "Compositor::UploadStaticImage called with null image");
        Assertf(image->GetWidth() > 0 && image->GetHeight() > 0,
            "Compositor::UploadStaticImage called with zero size image: %dx%d",
            image->GetWidth(),
            image->GetHeight());
        Assertf(image->GetComponents() == 4,
            "Compositor::UploadStaticImage called with unsupported component count: %d",
            image->GetComponents());
        auto view = device.LoadImage(image, genMipmap, srgb);
        device.FlushMainQueue();
        return view->Get();
    }

    rg::ResourceID Compositor::AddStaticImage(const rg::ResourceName &name, std::shared_ptr<GpuTexture> image) {
        ZoneScoped;
        Assertf(!name.empty(), "Compositor::AddStaticImage called with empty name");
        Assertf(!contains(name, '/'), "Compositor::AddStaticImage called with invalid name: %s", name);
        Assertf(image, "Compositor::AddStaticImage called with null image: %s", name);
        auto viewPtr = std::dynamic_pointer_cast<ImageView>(image);
        return graph.AddImageView("image:" + name, viewPtr);
    }

    void Compositor::UpdateSourceImage(ecs::Entity dst, std::shared_ptr<sp::Image> src) {
        ZoneScoped;
        if (src) {
            Assertf(src->GetWidth() > 0 && src->GetHeight() > 0,
                "Compositor::UpdateSourceImage called with zero size image: %dx%d",
                src->GetWidth(),
                src->GetHeight());
            Assertf(src->GetComponents() == 4,
                "Compositor::UpdateSourceImage called with unsupported component count: %d",
                src->GetComponents());
        }
        std::lock_guard l(dynamicSourceMutex);
        auto [it, _] = dynamicImageSources.emplace(dst, DynamicImageSource{});
        auto &source = it->second;
        source.cpuImage = src;
        source.cpuImageModified = true;
    }

    void Compositor::BeforeFrame(rg::RenderGraph &graph) {
        for (auto &output : renderOutputs) {
            if (!output.effectName.empty()) {
                if (!output.effectCondition) {
                    output.enableEffect = true;
                } else {
                    auto lock = ecs::StartTransaction<ecs::ReadSignalsLock>();
                    output.enableEffect = output.effectCondition.Evaluate(lock) >= 0.5;
                }
            } else {
                output.enableEffect = false;
            }
            if (output.guiContext) {
                output.guiContext->ClearEntities();
                if (!output.guiElements.empty()) {
                    auto lock = ecs::StartTransaction<ecs::Read<ecs::GuiElement>>();
                    for (auto &elementRef : output.guiElements) {
                        ecs::Entity ent = elementRef.Get(lock);
                        if (ent && ent.Has<ecs::GuiElement>(lock)) {
                            auto &guiElement = ent.Get<ecs::GuiElement>(lock);
                            if (guiElement.enabled && guiElement.definition) {
                                output.guiContext->AddEntity(ent,
                                    guiElement.definition,
                                    guiElement.anchor, // TODO: Move archor and preferred size to render output listing
                                    guiElement.preferredSize);
                            }
                        }
                    }
                }

                ImGui::SetCurrentContext(nullptr); // Don't leak contexts between render outputs
                output.guiContext->SetGuiContext();
                output.enableGui = output.guiContext->BeforeFrame(*this);
            } else {
                output.enableGui = false;
            }
        }

        {
            std::lock_guard l(dynamicSourceMutex);
            for (auto &[ent, source] : dynamicImageSources) {
                if (!source.cpuImage) continue;

                AsyncPtr<Image> latestReadyImage = nullptr;
                auto it = source.pendingUploads.begin();
                for (; it != source.pendingUploads.end(); it++) {
                    AsyncPtr<Image> &pendingImage = *it;
                    if (pendingImage && pendingImage->Ready()) {
                        latestReadyImage = *it;
                    } else {
                        break;
                    }
                }
                if (it != source.pendingUploads.begin()) {
                    source.pendingUploads.erase(source.pendingUploads.begin(), it);
                }

                if (latestReadyImage) {
                    ImageViewCreateInfo createInfo = {};
                    createInfo.image = latestReadyImage->Get();
                    createInfo.defaultSampler = createInfo.image->MipLevels() > 1
                                                    ? device.GetSampler(SamplerType::TrilinearClampEdge)
                                                    : device.GetSampler(SamplerType::BilinearClampEdge);
                    source.imageView = device.CreateImageView(createInfo);
                }
                if (source.imageView) {
                    ecs::EntityRef ref(ent);
                    if (ref) graph.AddImageView(rg::ResourceName("image:") + ref.Name().String(), source.imageView);
                }
            }
        }
    }

    void Compositor::UpdateRenderOutputs(ecs::Lock<ecs::Read<ecs::Name, ecs::RenderOutput>> lock) {
        renderOutputs.clear();
        existingOutputs.clear();
        viewRenderPassOffset = ~0llu;
        auto renderOutputEntities = lock.EntitiesWith<ecs::RenderOutput>();
        std::vector<ecs::Entity> unresolvedDependencies;
        unresolvedDependencies.reserve(renderOutputEntities.size());
        renderOutputs.reserve(renderOutputEntities.size());
        existingOutputs.reserve(renderOutputEntities.size());

        auto resolveDependency = [this, &lock](const ecs::Entity &ent, bool force = false) -> int {
            if (viewRenderPassOffset > renderOutputs.size() && ent.Has<ecs::View>(lock) && !force) {
                // View passes go as late as possible in the ordering, skip for now.
                return false;
            }

            AsyncPtr<ImageView> assetImage;
            auto &renderOutput = ent.Get<const ecs::RenderOutput>(lock);
            auto outputSize = renderOutput.outputSize;
            auto windowScale = renderOutput.scale;
            if (starts_with(renderOutput.sourceName, "/ent:")) {
                ecs::EntityRef sourceRef = ecs::Name(renderOutput.sourceName.substr(5), ecs::EntityScope());
                ecs::Entity sourceEnt = sourceRef.Get(lock);
                if (sourceEnt && sourceEnt != ent) {
                    auto it = existingOutputs.find(sourceEnt);
                    if (it != existingOutputs.end()) {
                        auto &existingInfo = renderOutputs.at(it->second);
                        if (outputSize.x <= 0.0f || outputSize.y <= 0.0f) outputSize = existingInfo.outputSize;
                        if (windowScale.x <= 0.0f || windowScale.y <= 0.0f) windowScale = existingInfo.scale;
                    } else if (!force) {
                        return false;
                    }
                }
            } else if (starts_with(renderOutput.sourceName, "asset:")) {
                auto assetName = std::string(renderOutput.sourceName.substr(6));
                if (!assetName.empty()) {
                    assetImage = staticAssetImages.Load(assetName);
                    if (!assetImage) {
                        assetImage = device.LoadAssetImage(assetName, true);
                        staticAssetImages.Register(assetName, assetImage, true);
                    }
                }
            }

            auto guiContext = renderOutput.guiContext.lock();
            if (!guiContext && !renderOutput.guiElements.empty()) {
                guiContext = ephemeralGuiContexts.Load(ent);
                if (!guiContext) {
                    guiContext = std::make_shared<GuiContext>(ent);
                    ephemeralGuiContexts.Register(ent, guiContext, true);
                }
            }

            // If outputSize is still -1 here, the compositor will inherit the source texture extents, otherwise (1, 1)
            if (windowScale.x <= 0.0f || windowScale.y <= 0.0f) windowScale = CVarWindowScale.Get();
            existingOutputs.emplace(ent, renderOutputs.size());
            renderOutputs.emplace_back(RenderOutputInfo{ent.Get<ecs::Name>(lock),
                ent,
                outputSize,
                windowScale,
                renderOutput.effectName,
                renderOutput.effectCondition,
                guiContext,
                true,
                true,
                renderOutput.guiElements,
                renderOutput.sourceName,
                assetImage});
            return true;
        };

        for (const ecs::Entity &ent : renderOutputEntities) {
            if (!resolveDependency(ent)) {
                unresolvedDependencies.emplace_back(ent);
            }
        }
        bool makingProgress = true;
        while (!unresolvedDependencies.empty() && makingProgress) {
            makingProgress = false;
            auto it = unresolvedDependencies.rbegin();
            while (it != unresolvedDependencies.rend()) {
                if (resolveDependency(*it)) {
                    // reverse_iterator -> iterator -> reverse_iterator
                    it = std::reverse_iterator(unresolvedDependencies.erase(std::next(it).base()));
                    makingProgress = true;
                } else {
                    it++;
                }
            }
            if (!makingProgress && viewRenderPassOffset > renderOutputs.size()) {
                viewRenderPassOffset = renderOutputs.size();
                makingProgress = true;
            } else if (!makingProgress) {
                // Force-solve dependency loops
                resolveDependency(unresolvedDependencies.back(), true);
                unresolvedDependencies.pop_back();
                makingProgress = true;
            }
        }
        if (!unresolvedDependencies.empty()) {
            Errorf("Unable to resolve render output source dependencies:");
            for (auto &ent : unresolvedDependencies) {
                auto &renderOutput = ent.Get<const ecs::RenderOutput>(lock);
                Errorf("    %s: source %s", ecs::ToString(lock, ent), renderOutput.sourceName);
            }
        }
    }

    void Compositor::AddOutputPasses(PassOrder order) {
        ZoneScoped;
        auto offset = std::min(viewRenderPassOffset, renderOutputs.size());
        std::span<RenderOutputInfo> outputSpan;
        switch (order) {
        case PassOrder::BeforeViews:
            outputSpan = {renderOutputs.begin(), renderOutputs.begin() + offset};
            break;
        case PassOrder::AfterViews:
            outputSpan = {renderOutputs.begin() + offset, renderOutputs.end()};
            break;
        default:
            Abortf("Compositor::AddOutputPasses called with invalid order: %s", order);
        }
        for (auto &output : outputSpan) {
            auto scope = graph.Scope("ent:" + output.entityName.String());
            // TODO:
            // - Implement asset: source inputs
            // - Implement reverse inheritance for menu and view to inherit from overlay/window
            // - Add crop / zoom / offset options
            // - Integrate with TransformSnapshot somehow to make sprite engine
            // - Remove extents from View and use RenderOutput instead
            // - Remove "cached" matrices from View and keep them in Renderer

            /**
            "render_output": {
                "output_size": [1280, 720],
                "background": [
                    {"view": "player:flatview"},
                    {
                        "effect": "background_blur",
                        "enable_if": "is_focused(Menu)"
                    }
                ]
            },
            "render_element": {
                {"gui": "menu:signal_display"},
                {
                    "gui": "menu:signal_display",
                    "offset": [10, 30],
                    "size": [100, 50]
                },
                {
                    "asset": "logos/sp_menu.png",
                    "offset": [10, 30],
                    "size": [100, 50]
                }
            }
             */

            rg::ResourceID viewOutput = rg::InvalidResource;
            rg::ResourceID sourceImageOutput = rg::InvalidResource;

            rg::ImageDesc outputDesc = {};

            graph.AddPass("RenderOutput")
                .Build([&](rg::PassBuilder &builder) {
                    bool inheritExtent = true;
                    outputDesc.format = vk::Format::eR8G8B8A8Srgb;
                    outputDesc.sampler = SamplerType::TrilinearClampEdge;

                    auto viewName = rg::ResourceName("view:") + output.entityName.String() + "/LastOutput";
                    viewOutput = builder.GetID(viewName, false);
                    if (viewOutput != rg::InvalidResource) {
                        builder.Read(viewOutput, Access::FragmentShaderSampleImage);
                        outputDesc = builder.DeriveImage(viewOutput);
                        inheritExtent = false;
                    }

                    if (output.outputSize.x > 0 && output.outputSize.y > 0) {
                        outputDesc.extent = vk::Extent3D(output.outputSize.x, output.outputSize.y, 1);
                        inheritExtent = false;
                    }

                    output.sourceResourceID = rg::InvalidResource;

                    if (starts_with(output.sourceName, "/ent:")) {
                        auto resourceID = builder.GetID(output.sourceName + "/LastOutput", false);
                        if (resourceID != rg::InvalidResource) {
                            builder.Read(resourceID, Access::FragmentShaderSampleImage);
                        } else {
                            resourceID = builder.ReadPreviousFrame(output.sourceName + "/LastOutput",
                                Access::FragmentShaderSampleImage);
                        }
                        if (resourceID != rg::InvalidResource) {
                            auto res = builder.GetResource(resourceID);
                            if (res.type == rg::Resource::Type::Image) {
                                auto derivedDesc = builder.DeriveImage(resourceID);
                                if (inheritExtent) {
                                    outputDesc.extent = derivedDesc.extent;
                                    inheritExtent = false;
                                }
                                outputDesc.sampler = derivedDesc.sampler;
                                output.sourceResourceID = resourceID;
                            } else {
                                output.sourceResourceID = builder.GetID("ErrorColor");
                            }
                        } else {
                            output.sourceResourceID = builder.GetID("ErrorColor");
                        }
                    } else if (starts_with(output.sourceName, "asset:") && output.assetImage) {
                        if (output.assetImage->Ready()) {
                            auto assetView = output.assetImage->Get();
                            if (assetView) {
                                if (inheritExtent) {
                                    outputDesc.extent = assetView->Extent();
                                    inheritExtent = false;
                                }
                                rg::ResourceName resourceName = "/" + output.sourceName;
                                std::transform(resourceName.begin() + 1,
                                    resourceName.end(),
                                    resourceName.begin() + 1,
                                    [](auto &ch) {
                                        if (ch == '/') return '_';
                                        return ch;
                                    });
                                output.sourceResourceID = graph.AddImageView(resourceName, assetView);
                                Assertf(output.sourceResourceID != rg::InvalidResource,
                                    "Failed to add asset image view to graph: %s",
                                    resourceName);
                            } else {
                                output.sourceResourceID = builder.GetID("ErrorColor");
                            }
                        }
                    }

                    auto sourceImageName = rg::ResourceName("image:") + output.entityName.String();
                    sourceImageOutput = builder.GetID(sourceImageName, false);
                    if (sourceImageOutput != rg::InvalidResource) {
                        builder.Read(sourceImageOutput, Access::FragmentShaderSampleImage);
                        if (inheritExtent) outputDesc = builder.DeriveImage(sourceImageOutput);
                        inheritExtent = false;
                    }

                    outputDesc.extent = vk::Extent3D(std::max(1u, outputDesc.extent.width),
                        std::max(1u, outputDesc.extent.height),
                        1);
                    outputDesc.mipLevels = CalculateMipmapLevels(outputDesc.extent);

                    builder.Read("ErrorColor", Access::FragmentShaderSampleImage);
                    builder.OutputColorAttachment(0, "RenderOutput", outputDesc, {LoadOp::Clear, StoreOp::Store});
                })
                .Execute([output, viewOutput, sourceImageOutput](rg::Resources &resources, CommandContext &cmd) {
                    cmd.SetDepthTest(false, false);
                    if (viewOutput != rg::InvalidResource) {
                        cmd.DrawScreenCover(resources.GetImageView(viewOutput));
                    }
                    cmd.SetBlending(true);

                    if (output.sourceResourceID != rg::InvalidResource) {
                        ImageViewPtr sourceImgView = resources.GetImageView(output.sourceResourceID);
                        if (sourceImgView) {
                            cmd.DrawScreenCover(sourceImgView);
                        } else {
                            cmd.DrawScreenCover(resources.GetImageView("ErrorColor"));
                        }
                    }

                    if (sourceImageOutput != rg::InvalidResource) {
                        ImageViewPtr sourceImgView = resources.GetImageView(sourceImageOutput);
                        if (sourceImgView) cmd.DrawScreenCover(sourceImgView);
                    }
                });

            if (!output.effectName.empty() && output.enableEffect) {
                if (output.effectName == "background_blur") {
                    renderer::AddBackgroundBlur(graph);
                }
            }

            ImGui::SetCurrentContext(nullptr); // Don't leak contexts between render outputs
            if (output.guiContext && output.enableGui && fontView->Ready()) {
                GuiContext &context = *output.guiContext;

                glm::ivec4 viewport(0, 0, outputDesc.extent.width, outputDesc.extent.height);
                DrawGuiContext(context, viewport, output.scale);
            }

            if (outputDesc.mipLevels > 1) renderer::AddMipmap(graph);
        }
    }

    void Compositor::EndFrame() {
        double currTime = (double)chrono_clock::now().time_since_epoch().count() / 1e9;
        deltaTime = lastTime > 0.0 ? (float)(currTime - lastTime) : 1.0f / 60.0f;
        lastTime = currTime;

        ephemeralGuiContexts.Tick(std::chrono::milliseconds(33));
        staticAssetImages.Tick(std::chrono::milliseconds(33));

        bool sourcesModified = false;
        {
            std::lock_guard l(dynamicSourceMutex);
            auto it = dynamicImageSources.begin();
            while (it != dynamicImageSources.end()) {
                auto &[ent, source] = *it;
                if (!source.cpuImage) {
                    it = dynamicImageSources.erase(it);
                    continue;
                }

                if (source.cpuImageModified) {
                    if (source.cpuImage) {
                        auto &cpuImage = *source.cpuImage;
                        Assertf(cpuImage.GetWidth() > 0 && cpuImage.GetHeight() > 0,
                            "Compositor uploading zero size image: %dx%d",
                            cpuImage.GetWidth(),
                            cpuImage.GetHeight());
                        Assertf(cpuImage.GetComponents() == 4,
                            "Unsupported number of source image components: %d",
                            cpuImage.GetComponents());
                        ImageCreateInfo createInfo = {};
                        createInfo.imageType = vk::ImageType::e2D;
                        createInfo.extent = vk::Extent3D(cpuImage.GetWidth(), cpuImage.GetHeight(), 1);
                        createInfo.mipLevels = CalculateMipmapLevels(createInfo.extent);
                        createInfo.format = vk::Format::eR8G8B8A8Unorm;
                        createInfo.usage = vk::ImageUsageFlagBits::eSampled;
                        createInfo.genMipmap = createInfo.mipLevels > 1;
                        source.pendingUploads.emplace_back(device.CreateImage(createInfo,
                            InitialData(cpuImage.GetImage().get(), cpuImage.ByteSize(), cpuImage.GetImage())));
                        sourcesModified = true;
                    }
                    source.cpuImageModified = false;
                }
                it++;
            }
        }
        if (sourcesModified) device.FlushMainQueue();
    }

    void Compositor::internalDrawGui(const GuiDrawData &drawData, vk::Rect2D viewport, glm::vec2 scale) {
        if (drawData.drawCommands.empty()) return;
        graph.AddPass("GuiRender")
            .Build([&](rg::PassBuilder &builder) {
                for (const auto &cmd : drawData.drawCommands) {
                    if (cmd.textureId == FontAtlasID) continue;
                    if (cmd.textureId <= std::numeric_limits<rg::ResourceID>::max()) {
                        rg::ResourceID resourceID = (rg::ResourceID)cmd.textureId;
                        if (resourceID != rg::InvalidResource) {
                            builder.Read(resourceID, Access::FragmentShaderSampleImage);
                        }
                    }
                }
                builder.Read("ErrorColor", Access::FragmentShaderSampleImage);
                builder.SetColorAttachment(0, builder.LastOutputID(), {LoadOp::Load, StoreOp::Store});
            })
            .Execute([this, drawData, viewport, scale](rg::Resources &resources, CommandContext &cmd) {
                size_t totalVtxSize = CeilToPowerOfTwo(drawData.vertexBuffer.size() * sizeof(GuiDrawVertex));
                size_t totalIdxSize = CeilToPowerOfTwo(drawData.indexBuffer.size() * sizeof(GuiDrawIndex));
                if (totalVtxSize == 0 || totalIdxSize == 0) return;

                BufferDesc vtxDesc;
                vtxDesc.layout = totalVtxSize;
                vtxDesc.usage = vk::BufferUsageFlagBits::eVertexBuffer;
                vtxDesc.residency = Residency::CPU_TO_GPU;
                auto vertexBuffer = cmd.Device().GetBuffer(vtxDesc);

                BufferDesc idxDesc;
                idxDesc.layout = totalIdxSize;
                idxDesc.usage = vk::BufferUsageFlagBits::eIndexBuffer;
                idxDesc.residency = Residency::CPU_TO_GPU;
                auto indexBuffer = cmd.Device().GetBuffer(idxDesc);

                GuiDrawVertex *vtxData;
                GuiDrawIndex *idxData;
                vertexBuffer->Map((void **)&vtxData);
                indexBuffer->Map((void **)&idxData);
                std::copy(drawData.vertexBuffer.begin(), drawData.vertexBuffer.end(), vtxData);
                std::copy(drawData.indexBuffer.begin(), drawData.indexBuffer.end(), idxData);
                indexBuffer->Unmap();
                vertexBuffer->Unmap();

                cmd.SetYDirection(YDirection::Down);
                cmd.SetViewport(viewport);
                cmd.SetVertexLayout(*vertexLayout);
                cmd.SetCullMode(vk::CullModeFlagBits::eNone);
                cmd.SetDepthTest(false, false);
                cmd.SetBlending(true);

                cmd.SetShaders("basic_ortho.vert", "single_texture.frag");

                auto zeroViewport = viewport;
                zeroViewport.offset = vk::Offset2D();
                glm::mat4 proj = MakeOrthographicProjection(YDirection::Down, zeroViewport, scale);
                cmd.PushConstants(proj);

                const vk::IndexType idxType = sizeof(GuiDrawIndex) == 2 ? vk::IndexType::eUint16
                                                                        : vk::IndexType::eUint32;
                cmd.Raw().bindIndexBuffer(*indexBuffer, 0, idxType);
                cmd.Raw().bindVertexBuffers(0, {*vertexBuffer}, {0});

                uint32 idxOffset = 0;
                for (const auto &pcmd : drawData.drawCommands) {
                    if (pcmd.textureId == FontAtlasID) {
                        cmd.SetImageView("tex", fontView->Get());
                    } else if (pcmd.textureId <= std::numeric_limits<rg::ResourceID>::max()) {
                        rg::ResourceID resourceID = (rg::ResourceID)pcmd.textureId;
                        if (resourceID != rg::InvalidResource) {
                            cmd.SetImageView("tex", resourceID);
                        } else {
                            cmd.SetImageView("tex", "ErrorColor");
                        }
                    } else {
                        cmd.SetImageView("tex", "ErrorColor");
                    }
                    glm::ivec2 clipOffset(pcmd.clipRect.x, pcmd.clipRect.y);
                    glm::uvec2 clipExtents(pcmd.clipRect.z - pcmd.clipRect.x, pcmd.clipRect.w - pcmd.clipRect.y);
                    glm::uvec2 viewportExtents(viewport.extent.width, viewport.extent.height);
                    clipOffset = glm::clamp(clipOffset + glm::ivec2(viewport.offset.x, viewport.offset.y),
                        glm::ivec2(0),
                        glm::ivec2(viewportExtents) - 1);
                    clipExtents = glm::clamp(clipExtents, glm::uvec2(0), viewportExtents - glm::uvec2(clipOffset));

                    cmd.SetScissor(vk::Rect2D{{(int32)clipOffset.x, (int32)clipOffset.y},
                        {(uint32)clipExtents.x, (uint32)clipExtents.y}});

                    cmd.DrawIndexed(pcmd.indexCount, 1, idxOffset, pcmd.vertexOffset, 0);
                    idxOffset += pcmd.indexCount;
                }

                cmd.ClearScissor();
            });
    }

    void Compositor::internalDrawGui(ImDrawData *drawData,
        vk::Rect2D viewport,
        glm::vec2 scale,
        bool allowUserCallback) {
        if (!drawData) return;
        graph.AddPass("GuiRender")
            .Build([&](rg::PassBuilder &builder) {
                for (const auto &cmdList : drawData->CmdLists) {
                    for (const auto &cmd : cmdList->CmdBuffer) {
                        if (cmd.UserCallback) {
                            Assertf(allowUserCallback, "ImGui UserCallback on render not allowed");
                            cmd.UserCallback(cmdList, &cmd);
                        } else if (cmd.TextureId == FontAtlasID) {
                            continue;
                        } else if (cmd.TextureId <= std::numeric_limits<rg::ResourceID>::max()) {
                            rg::ResourceID resourceID = (rg::ResourceID)cmd.TextureId;
                            if (resourceID != rg::InvalidResource) {
                                builder.Read(resourceID, Access::FragmentShaderSampleImage);
                            }
                        }
                    }
                }
                builder.Read("ErrorColor", Access::FragmentShaderSampleImage);
                builder.SetColorAttachment(0, builder.LastOutputID(), {LoadOp::Load, StoreOp::Store});
            })
            .Execute([this, drawData, viewport, scale](rg::Resources &resources, CommandContext &cmd) {
                size_t totalVtxSize = 0, totalIdxSize = 0;
                for (const auto &cmdList : drawData->CmdLists) {
                    totalVtxSize += cmdList->VtxBuffer.size_in_bytes();
                    totalIdxSize += cmdList->IdxBuffer.size_in_bytes();
                }

                totalVtxSize = CeilToPowerOfTwo(totalVtxSize);
                totalIdxSize = CeilToPowerOfTwo(totalIdxSize);
                if (totalVtxSize == 0 || totalIdxSize == 0) return;

                BufferDesc vtxDesc;
                vtxDesc.layout = totalVtxSize;
                vtxDesc.usage = vk::BufferUsageFlagBits::eVertexBuffer;
                vtxDesc.residency = Residency::CPU_TO_GPU;
                auto vertexBuffer = cmd.Device().GetBuffer(vtxDesc);

                BufferDesc idxDesc;
                idxDesc.layout = totalIdxSize;
                idxDesc.usage = vk::BufferUsageFlagBits::eIndexBuffer;
                idxDesc.residency = Residency::CPU_TO_GPU;
                auto indexBuffer = cmd.Device().GetBuffer(idxDesc);

                ImDrawVert *vtxData;
                ImDrawIdx *idxData;
                vertexBuffer->Map((void **)&vtxData);
                indexBuffer->Map((void **)&idxData);
                for (const auto &cmdList : drawData->CmdLists) {
                    std::copy(cmdList->VtxBuffer.begin(), cmdList->VtxBuffer.end(), vtxData);
                    vtxData += cmdList->VtxBuffer.size();
                    std::copy(cmdList->IdxBuffer.begin(), cmdList->IdxBuffer.end(), idxData);
                    idxData += cmdList->IdxBuffer.size();
                }
                indexBuffer->Unmap();
                vertexBuffer->Unmap();

                cmd.SetYDirection(YDirection::Down);
                cmd.SetViewport(viewport);
                cmd.SetVertexLayout(*vertexLayout);
                cmd.SetCullMode(vk::CullModeFlagBits::eNone);
                cmd.SetDepthTest(false, false);
                cmd.SetBlending(true);

                cmd.SetShaders("basic_ortho.vert", "single_texture.frag");

                glm::mat4 proj = MakeOrthographicProjection(YDirection::Down, viewport, scale);
                cmd.PushConstants(proj);

                const vk::IndexType idxType = sizeof(ImDrawIdx) == 2 ? vk::IndexType::eUint16 : vk::IndexType::eUint32;
                cmd.Raw().bindIndexBuffer(*indexBuffer, 0, idxType);
                cmd.Raw().bindVertexBuffers(0, {*vertexBuffer}, {0});

                uint32 idxOffset = 0, vtxOffset = 0;
                for (const auto &cmdList : drawData->CmdLists) {
                    for (const auto &pcmd : cmdList->CmdBuffer) {
                        if (pcmd.UserCallback) continue;
                        if (pcmd.TextureId == FontAtlasID) {
                            cmd.SetImageView("tex", fontView->Get());
                        } else if (pcmd.TextureId <= std::numeric_limits<rg::ResourceID>::max()) {
                            rg::ResourceID resourceID = (rg::ResourceID)pcmd.TextureId;
                            if (resourceID != rg::InvalidResource) {
                                cmd.SetImageView("tex", resourceID);
                            } else {
                                cmd.SetImageView("tex", "ErrorColor");
                            }
                        } else {
                            cmd.SetImageView("tex", "ErrorColor");
                        }

                        auto clipRect = pcmd.ClipRect;
                        clipRect.x -= drawData->DisplayPos.x;
                        clipRect.y -= drawData->DisplayPos.y;
                        clipRect.z -= drawData->DisplayPos.x;
                        clipRect.w -= drawData->DisplayPos.y;
                        // TODO: Clamp to viewport

                        cmd.SetScissor(vk::Rect2D{{(int32)clipRect.x, (int32)clipRect.y},
                            {(uint32)(clipRect.z - clipRect.x), (uint32)(clipRect.w - clipRect.y)}});

                        cmd.DrawIndexed(pcmd.ElemCount, 1, idxOffset, vtxOffset, 0);
                        idxOffset += pcmd.ElemCount;
                    }
                    vtxOffset += cmdList->VtxBuffer.size();
                }

                cmd.ClearScissor();
            });
    }
} // namespace sp::vulkan
