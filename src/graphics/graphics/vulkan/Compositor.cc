/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Compositor.hh"

#include "assets/Asset.hh"
#include "assets/AssetManager.hh"
#include "common/Defer.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/ScriptGuiDefinition.hh"
#include "ecs/ScriptManager.hh"
#include "graphics/gui/GuiContext.hh"
#include "graphics/vulkan/Renderer.hh"
#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/core/Util.hh"
#include "graphics/vulkan/core/VkCommon.hh"
#include "graphics/vulkan/render_passes/Mipmap.hh"
#include "graphics/vulkan/scene/VertexLayouts.hh"

#include <algorithm>
#include <imgui/imgui.h>

namespace sp::vulkan {
    constexpr ImTextureID FONT_ATLAS_ID = 1ull << (sizeof(TextureIndex) * 8);

    Compositor::Compositor(DeviceContext &device, Renderer &renderer)
        : renderer(renderer), scene(renderer.scene), graph(renderer.graph) {
        vertexLayout = make_unique<VertexLayout>(0, sizeof(ImDrawVert));
        vertexLayout->PushAttribute(0, 0, vk::Format::eR32G32Sfloat, offsetof(ImDrawVert, pos));
        vertexLayout->PushAttribute(1, 0, vk::Format::eR32G32Sfloat, offsetof(ImDrawVert, uv));
        vertexLayout->PushAttribute(2, 0, vk::Format::eR8G8B8A8Unorm, offsetof(ImDrawVert, col));

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

        ImageViewCreateInfo fontViewInfo;
        fontViewInfo.defaultSampler = device.GetSampler(SamplerType::BilinearClampEdge);

        fontView = device.CreateImageAndView(fontImageInfo,
            fontViewInfo,
            make_async<InitialData>(fontData, size_t(fontWidth * fontHeight * 4)));

        EndFrame();
    }

    void Compositor::DrawGui(ImDrawData *drawData, glm::ivec4 viewport, glm::vec2 scale) {
        Assertf(viewport.z > 0 && viewport.w > 0,
            "Compositor::DrawGui called with invalid viewport: %s",
            glm::to_string(viewport));
        internalDrawGui(drawData,
            vk::Rect2D({viewport.x, viewport.y}, {(uint32_t)viewport.z, (uint32_t)viewport.w}),
            scale,
            false);
    }

    void Compositor::internalDrawGui(ImDrawData *drawData,
        vk::Rect2D viewport,
        glm::vec2 scale,
        bool allowUserCallback) {
        if (!drawData) return;
        graph.AddPass("GuiRender")
            .Build([&](rg::PassBuilder &builder) {
                builder.SetColorAttachment(0, builder.LastOutputID(), {LoadOp::Load, StoreOp::Store});
            })
            .Execute([this, drawData, viewport, scale, allowUserCallback](rg::Resources &resources,
                         CommandContext &cmd) {
                size_t totalVtxSize = 0, totalIdxSize = 0;
                for (int i = 0; i < drawData->CmdListsCount; i++) {
                    const auto cmdList = drawData->CmdLists[i];
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
                for (int i = 0; i < drawData->CmdListsCount; i++) {
                    const auto cmdList = drawData->CmdLists[i];
                    std::copy(cmdList->VtxBuffer.begin(), cmdList->VtxBuffer.end(), vtxData);
                    vtxData += cmdList->VtxBuffer.size();
                    std::copy(cmdList->IdxBuffer.begin(), cmdList->IdxBuffer.end(), idxData);
                    idxData += cmdList->IdxBuffer.size();
                }
                indexBuffer->Unmap();
                vertexBuffer->Unmap();

                cmd.SetViewport(viewport);
                cmd.SetVertexLayout(*vertexLayout);
                cmd.SetCullMode(vk::CullModeFlagBits::eNone);
                cmd.SetDepthTest(false, false);
                cmd.SetBlending(true);

                cmd.SetShaders("basic_ortho.vert", "single_texture.frag");

                glm::mat4 proj = MakeOrthographicProjection(viewport, scale);
                cmd.PushConstants(proj);

                const vk::IndexType idxType = sizeof(ImDrawIdx) == 2 ? vk::IndexType::eUint16 : vk::IndexType::eUint32;
                cmd.Raw().bindIndexBuffer(*indexBuffer, 0, idxType);
                cmd.Raw().bindVertexBuffers(0, {*vertexBuffer}, {0});

                uint32 idxOffset = 0, vtxOffset = 0;
                for (int i = 0; i < drawData->CmdListsCount; i++) {
                    const auto *cmdList = drawData->CmdLists[i];

                    for (const auto &pcmd : cmdList->CmdBuffer) {
                        if (pcmd.UserCallback) {
                            Assertf(allowUserCallback, "ImGui UserCallback on render not allowed");
                            pcmd.UserCallback(cmdList, &pcmd);
                        } else {
                            if (pcmd.TextureId == FONT_ATLAS_ID) {
                                cmd.SetImageView("tex", fontView->Get());
                            } else if (pcmd.TextureId < scene.textures.Count()) {
                                auto imgPtr = scene.textures.Get(pcmd.TextureId);
                                if (imgPtr) {
                                    cmd.SetImageView("tex", imgPtr);
                                } else {
                                    cmd.SetImageView("tex", scene.textures.GetSinglePixel(ERROR_COLOR));
                                }
                            } else if ((pcmd.TextureId >> 32) == 0xFFFFFFFF) {
                                rg::ResourceID resourceID = pcmd.TextureId & (1ull << (sizeof(rg::ResourceID) * 8)) - 1;
                                if (resourceID != rg::InvalidResource) {
                                    cmd.SetImageView("tex", resourceID);
                                } else {
                                    cmd.SetImageView("tex", scene.textures.GetSinglePixel(ERROR_COLOR));
                                }
                            } else {
                                cmd.SetImageView("tex", scene.textures.GetSinglePixel(ERROR_COLOR));
                            }

                            auto clipRect = pcmd.ClipRect;
                            clipRect.x -= drawData->DisplayPos.x;
                            clipRect.y -= drawData->DisplayPos.y;
                            clipRect.z -= drawData->DisplayPos.x;
                            clipRect.w -= drawData->DisplayPos.y;
                            // TODO: Clamp to viewport

                            cmd.SetScissor(vk::Rect2D{{(int32)clipRect.x, (int32)(viewport.extent.height - clipRect.w)},
                                {(uint32)(clipRect.z - clipRect.x), (uint32)(clipRect.w - clipRect.y)}});

                            cmd.DrawIndexed(pcmd.ElemCount, 1, idxOffset, vtxOffset, 0);
                        }
                        idxOffset += pcmd.ElemCount;
                    }
                    vtxOffset += cmdList->VtxBuffer.size();
                }

                cmd.ClearScissor();
            });
    }

    void Compositor::BeforeFrame() {
        for (auto &output : renderOutputs) {
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

                output.enableGui = output.guiContext->BeforeFrame();
            } else {
                output.enableGui = false;
            }
        }
    }

    void Compositor::UpdateRenderOutputs(ecs::Lock<ecs::Read<ecs::Name>, ecs::Write<ecs::RenderOutput>> lock) {
        renderOutputs.clear();
        auto renderOutputEntities = lock.EntitiesWith<ecs::RenderOutput>();
        robin_hood::unordered_set<ecs::Entity> existingOutputs;
        std::vector<RenderOutputInfo> unresolvedDependencies;
        unresolvedDependencies.reserve(renderOutputEntities.size());
        renderOutputs.reserve(renderOutputEntities.size());
        existingOutputs.reserve(renderOutputEntities.size());

        auto dependencyResolved = [&lock, &existingOutputs](const RenderOutputInfo &info) {
            if (!starts_with(info.sourceName, "ent:")) return true;
            ecs::EntityRef ref = ecs::Name(info.sourceName.substr(4), ecs::EntityScope());
            ecs::Entity ent = ref.Get(lock);
            if (!ent || ent == info.entity) return true;
            return existingOutputs.count(ent) > 0;
        };

        for (const ecs::Entity &ent : renderOutputEntities) {
            ecs::EntityRef ref(ent);
            auto &renderOutput = ent.Get<const ecs::RenderOutput>(lock);
            if (!renderOutput.guiContext && !renderOutput.guiElements.empty()) {
                ent.Get<ecs::RenderOutput>(lock).guiContext = std::make_shared<GuiContext>(ref);
            }

            auto windowScale = renderOutput.scale;
            if (windowScale.x <= 0.0f || windowScale.y <= 0.0f) windowScale = CVarWindowScale.Get();
            if (windowScale.x <= 0.0f) windowScale.x = 1.0f;
            if (windowScale.y <= 0.0f) windowScale.y = windowScale.x;
            RenderOutputInfo info = {ref.Name(),
                ent,
                renderOutput.outputSize,
                windowScale,
                renderOutput.guiContext,
                true,
                renderOutput.guiElements,
                renderOutput.sourceName};
            if (dependencyResolved(info)) {
                existingOutputs.emplace(info.entity);
                renderOutputs.push_back(std::move(info));
            } else {
                unresolvedDependencies.push_back(std::move(info));
            }
        }
        bool makingProgress = true;
        while (!unresolvedDependencies.empty() && makingProgress) {
            makingProgress = false;
            auto it = unresolvedDependencies.rbegin();
            while (it != unresolvedDependencies.rend()) {
                auto &info = *it;
                if (dependencyResolved(info)) {
                    existingOutputs.emplace(info.entity);
                    renderOutputs.push_back(std::move(info));
                    // reverse_iterator -> iterator -> reverse_iterator
                    it = std::reverse_iterator(unresolvedDependencies.erase(std::next(it).base()));
                    makingProgress = true;
                } else {
                    it++;
                }
            }
        }
        if (!unresolvedDependencies.empty()) {
            Errorf("Unable to resolve render output source dependencies:");
            for (auto &info : unresolvedDependencies) {
                Errorf("    %s: source %s", info.entityName, info.sourceName);
            }
        }
    }

    void Compositor::AddOutputPasses() {
        ZoneScoped;
        for (auto &output : renderOutputs) {
            // TODO:
            // - Handle scaling background / passing through aspect ratio
            // - Add effect shader options (menu blur)
            // - Improve asset / render output / graph string references
            // - Remove extents from View and use RenderOutput instead
            // - Remove "cached" matrices from View and keep them in Renderer

            auto viewName = rg::ResourceName("view:") + output.entityName.String() + ".LastOutput";
            rg::ResourceID viewOutput = rg::InvalidResource;

            rg::ImageDesc outputDesc;
            graph.AddPass("RenderOutput")
                .Build([&](rg::PassBuilder &builder) {
                    bool inheritExtent = true;
                    outputDesc.format = vk::Format::eR8G8B8A8Srgb;
                    outputDesc.sampler = SamplerType::BilinearClampEdge;

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

                    output.sourceTexture = rg::InvalidResource;

                    if (starts_with(output.sourceName, "ent:")) {
                        auto resourceID = builder.GetID(output.sourceName, false);
                        if (resourceID != rg::InvalidResource) {
                            builder.Read(resourceID, Access::FragmentShaderSampleImage);
                            output.sourceTexture = resourceID;
                            auto derivedDesc = builder.DeriveImage(resourceID);
                            if (inheritExtent) outputDesc.extent = derivedDesc.extent;
                            outputDesc.sampler = derivedDesc.sampler;
                        }
                    } else if (auto it = scene.liveTextureCache.find(output.sourceName);
                        it != scene.liveTextureCache.end()) {
                        if (starts_with(it->first, "graph:")) {
                            auto resourceID = builder.GetID(output.sourceName.substr(6), false);
                            if (resourceID != rg::InvalidResource) {
                                builder.Read(resourceID, Access::FragmentShaderSampleImage);
                            } else {
                                resourceID = builder.ReadPreviousFrame(output.sourceName.substr(6),
                                    Access::FragmentShaderSampleImage);
                            }
                            if (resourceID != rg::InvalidResource) {
                                auto resource = builder.GetResource(resourceID);
                                if (resource.type == rg::Resource::Type::Image) {
                                    output.sourceTexture = resourceID;
                                    auto derivedDesc = builder.DeriveImage(resourceID);
                                    if (inheritExtent) outputDesc.extent = derivedDesc.extent;
                                    outputDesc.sampler = derivedDesc.sampler;
                                }
                            }
                        } else if (it->second.Ready()) {
                            output.sourceTexture = it->second.index;
                            if (inheritExtent) {
                                auto sourceView = scene.textures.Get(it->second.index);
                                if (sourceView) outputDesc.extent = sourceView->Extent();
                            }
                        }
                    }

                    outputDesc.extent = vk::Extent3D(std::max(1u, outputDesc.extent.width),
                        std::max(1u, outputDesc.extent.height),
                        1);
                    outputDesc.mipLevels = CalculateMipmapLevels(outputDesc.extent);

                    auto name = "ent:" + output.entityName.String();
                    auto target = builder.OutputColorAttachment(0, name, outputDesc, {LoadOp::Clear, StoreOp::Store});
                    output.renderGraphID = target.id;
                })
                .Execute([this, outputDesc, output, viewOutput](rg::Resources &resources, CommandContext &cmd) {
                    if (viewOutput != rg::InvalidResource) {
                        cmd.DrawScreenCover(resources.GetImageView(viewOutput));
                    }
                    cmd.SetBlending(true);

                    ImageViewPtr sourceImgView;
                    if (std::holds_alternative<rg::ResourceID>(output.sourceTexture)) {
                        auto &resourceID = std::get<rg::ResourceID>(output.sourceTexture);
                        if (resourceID != rg::InvalidResource) {
                            sourceImgView = resources.GetImageView(resourceID);
                            if (!sourceImgView) {
                                sourceImgView = scene.textures.GetSinglePixel(ERROR_COLOR);
                            }
                        }
                    } else {
                        sourceImgView = scene.textures.Get(std::get<TextureIndex>(output.sourceTexture));
                        if (!sourceImgView) {
                            sourceImgView = scene.textures.GetSinglePixel(ERROR_COLOR);
                        }
                    }
                    if (sourceImgView) cmd.DrawScreenCover(sourceImgView);
                });
            if (outputDesc.mipLevels > 1) {
                renderer::AddMipmap(graph, output.renderGraphID);
            }
            // TODO: Add effects passes

            if (output.guiContext && output.enableGui && fontView->Ready()) {
                GuiContext &context = *output.guiContext;

                vk::Rect2D viewport = {{}, {outputDesc.extent.width, outputDesc.extent.height}};
                if (context.SetGuiContext()) {
                    ImGui::GetMainViewport()->PlatformHandleRaw = renderer.device.Win32WindowHandle();

                    ImGuiIO &io = ImGui::GetIO();
                    void *oldUserData = io.UserData;
                    io.UserData = (GenericCompositor *)this;
                    io.IniFilename = nullptr;
                    io.DisplaySize = ImVec2(viewport.extent.width / output.scale.x,
                        viewport.extent.height / output.scale.y);
                    io.DisplayFramebufferScale = ImVec2(output.scale.x, output.scale.y);
                    io.DeltaTime = deltaTime;

                    auto lastFonts = io.Fonts;
                    io.Fonts = fontAtlas.get();
                    io.Fonts->TexID = FONT_ATLAS_ID;
                    Defer defer([&] {
                        io.Fonts = lastFonts;
                    });

                    ImGui::NewFrame();
                    context.DefineWindows();
                    ImGui::Render();

                    io.UserData = oldUserData;

                    ImDrawData *drawData = ImGui::GetDrawData();
                    drawData->ScaleClipRects(io.DisplayFramebufferScale);
                    internalDrawGui(drawData, viewport, output.scale, true);
                } else {
                    ImDrawData *drawData = context.GetDrawData(
                        glm::vec2(viewport.extent.width / output.scale.x, viewport.extent.height / output.scale.y),
                        output.scale,
                        deltaTime);
                    drawData->ScaleClipRects(ImVec2(output.scale.x, output.scale.y));
                    internalDrawGui(drawData, viewport, output.scale, false);
                }
            }
        }
    }

    void Compositor::EndFrame() {
        double currTime = (double)chrono_clock::now().time_since_epoch().count() / 1e9;
        deltaTime = lastTime > 0.0 ? (float)(currTime - lastTime) : 1.0f / 60.0f;
        lastTime = currTime;
    }

} // namespace sp::vulkan
