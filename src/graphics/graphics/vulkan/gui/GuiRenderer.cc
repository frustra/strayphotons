/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "GuiRenderer.hh"

#include "assets/Asset.hh"
#include "assets/AssetManager.hh"
#include "core/Defer.hh"
#include "graphics/gui/GuiContext.hh"
#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/core/Util.hh"
#include "graphics/vulkan/core/VkCommon.hh"
#include "graphics/vulkan/scene/VertexLayouts.hh"

#include <algorithm>
#include <imgui/imgui.h>

namespace sp::vulkan {
    GuiRenderer::GuiRenderer(DeviceContext &device) {
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

        for (auto &def : GetFontList()) {
            auto asset = Assets()->Load("fonts/"s + def.name)->Get();
            Assertf(asset, "Failed to load gui font %s", def.name);

            ImFontConfig cfg;
            cfg.FontData = (void *)asset->BufferPtr();
            cfg.FontDataSize = asset->BufferSize();
            cfg.FontDataOwnedByAtlas = false;
            cfg.SizePixels = def.size;
            cfg.GlyphRanges = &glyphRanges[0];
            auto filename = asset->path.filename().string();
            memcpy(cfg.Name, filename.c_str(), std::min(sizeof(cfg.Name), filename.length()));
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
            {fontData, size_t(fontWidth * fontHeight * 4)});

        Tick();
    }

    void GuiRenderer::Tick() {
        double currTime = (double)chrono_clock::now().time_since_epoch().count() / 1e9;
        deltaTime = lastTime > 0.0 ? (float)(currTime - lastTime) : 1.0f / 60.0f;
        lastTime = currTime;
    }

    void GuiRenderer::Render(GuiContext &context, CommandContext &cmd, vk::Rect2D viewport) {
        if (!fontView->Ready()) return;
        ZoneScoped;

        context.SetGuiContext();
        ImGui::GetMainViewport()->PlatformHandleRaw = cmd.Device().Win32WindowHandle();

        ImGuiIO &io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = ImVec2(viewport.extent.width, viewport.extent.height);
        io.DeltaTime = deltaTime;

        auto lastFonts = io.Fonts;
        io.Fonts = fontAtlas.get();
        io.Fonts->TexID = (ImTextureID)(fontView->Get()->GetHandle());
        Defer defer([&] {
            io.Fonts = lastFonts;
        });

        ImGui::NewFrame();
        context.DefineWindows();
        ImGui::Render();

        auto drawData = ImGui::GetDrawData();
        drawData->ScaleClipRects(io.DisplayFramebufferScale);

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
        cmd.SetBlending(true, vk::BlendOp::eAdd);
        cmd.SetBlendFunc(vk::BlendFactor::eSrcAlpha, vk::BlendFactor::eOneMinusSrcAlpha);

        cmd.SetShaders("basic_ortho.vert", "single_texture.frag");

        glm::mat4 proj = MakeOrthographicProjection(viewport);
        cmd.PushConstants(proj);

        const vk::IndexType idxType = sizeof(ImDrawIdx) == 2 ? vk::IndexType::eUint16 : vk::IndexType::eUint32;
        cmd.Raw().bindIndexBuffer(*indexBuffer, 0, idxType);
        cmd.Raw().bindVertexBuffers(0, {*vertexBuffer}, {0});

        uint32 idxOffset = 0, vtxOffset = 0;
        for (int i = 0; i < drawData->CmdListsCount; i++) {
            const auto cmdList = drawData->CmdLists[i];

            for (const auto &pcmd : cmdList->CmdBuffer) {
                if (pcmd.UserCallback) {
                    pcmd.UserCallback(cmdList, &pcmd);
                } else {
                    auto texture = ImageView::FromHandle((uintptr_t)pcmd.TextureId);
                    cmd.SetImageView(0, 0, texture);

                    auto clipRect = pcmd.ClipRect;
                    clipRect.x -= drawData->DisplayPos.x;
                    clipRect.y -= drawData->DisplayPos.y;
                    clipRect.z -= drawData->DisplayPos.x;
                    clipRect.w -= drawData->DisplayPos.y;

                    cmd.SetScissor(vk::Rect2D{{(int32)clipRect.x, (int32)(viewport.extent.height - clipRect.w)},
                        {(uint32)(clipRect.z - clipRect.x), (uint32)(clipRect.w - clipRect.y)}});

                    cmd.DrawIndexed(pcmd.ElemCount, 1, idxOffset, vtxOffset, 0);
                }
                idxOffset += pcmd.ElemCount;
            }
            vtxOffset += cmdList->VtxBuffer.size();
        }

        cmd.ClearScissor();
    }

} // namespace sp::vulkan
