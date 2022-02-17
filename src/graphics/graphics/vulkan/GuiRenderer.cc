#include "GuiRenderer.hh"

#include "assets/Asset.hh"
#include "assets/AssetManager.hh"
#include "graphics/gui/GuiManager.hh"
#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/Common.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/core/Util.hh"
#include "graphics/vulkan/core/Vertex.hh"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <imgui/imgui.h>

namespace sp::vulkan {
    GuiRenderer::GuiRenderer(DeviceContext &device, GuiManager &manager) : device(device), manager(manager) {
        manager.SetGuiContext();
        ImGuiIO &io = ImGui::GetIO();
        io.IniFilename = nullptr;

        ImGui::GetMainViewport()->PlatformHandleRaw = device.Win32WindowHandle();

        std::pair<shared_ptr<const Asset>, float> fontAssets[] = {
            std::make_pair(GAssets.Load("fonts/DroidSans.ttf")->Get(), 16.0f),
            std::make_pair(GAssets.Load("fonts/3270Medium.ttf")->Get(), 32.0f),
            std::make_pair(GAssets.Load("fonts/3270Medium.ttf")->Get(), 25.0f),
        };

        io.Fonts->AddFontDefault(nullptr);

        static const ImWchar glyphRanges[] = {
            0x0020,
            0x00FF, // Basic Latin + Latin Supplement
            0x2100,
            0x214F, // Letterlike Symbols
            0,
        };

        for (auto &pair : fontAssets) {
            auto &asset = pair.first;
            Assert(asset, "Failed to load gui font");
            ImFontConfig cfg;
            cfg.FontData = (void *)asset->BufferPtr();
            cfg.FontDataSize = asset->BufferSize();
            cfg.FontDataOwnedByAtlas = false;
            cfg.SizePixels = pair.second;
            cfg.GlyphRanges = &glyphRanges[0];
            memcpy(cfg.Name, asset->path.c_str(), std::min(sizeof(cfg.Name), asset->path.length()));
            io.Fonts->AddFont(&cfg);
        }

        vertexLayout = make_unique<VertexLayout>(0, sizeof(ImDrawVert));
        vertexLayout->PushAttribute(0, 0, vk::Format::eR32G32Sfloat, offsetof(ImDrawVert, pos));
        vertexLayout->PushAttribute(1, 0, vk::Format::eR32G32Sfloat, offsetof(ImDrawVert, uv));
        vertexLayout->PushAttribute(2, 0, vk::Format::eR8G8B8A8Unorm, offsetof(ImDrawVert, col));
    }

    void GuiRenderer::Render(CommandContext &cmd, vk::Rect2D viewport) {
        manager.SetGuiContext();
        ImGuiIO &io = ImGui::GetIO();

        if (!fontView) {
            // TODO: don't duplicate font image for every gui, just build it once
            uint8 *fontData;
            int fontWidth, fontHeight;
            io.Fonts->GetTexDataAsRGBA32(&fontData, &fontWidth, &fontHeight);

            ImageCreateInfo fontImageInfo;
            fontImageInfo.imageType = vk::ImageType::e2D;
            fontImageInfo.extent = vk::Extent3D{(uint32)fontWidth, (uint32)fontHeight, 1};
            fontImageInfo.format = vk::Format::eR8G8B8A8Unorm;
            fontImageInfo.usage = vk::ImageUsageFlagBits::eSampled;

            ImageViewCreateInfo fontViewInfo;
            fontViewInfo.defaultSampler = device.GetSampler(SamplerType::BilinearClamp);

            auto fut = device.CreateImageAndView(fontImageInfo,
                fontViewInfo,
                {fontData, size_t(fontWidth * fontHeight * 4)});
            device.FlushMainQueue();
            fontView = fut->Get();

            io.Fonts->TexID = (ImTextureID)(fontView->GetHandle());
        }

        io.DisplaySize = ImVec2(viewport.extent.width, viewport.extent.height);

        double currTime = glfwGetTime();
        io.DeltaTime = lastTime > 0.0 ? (float)(currTime - lastTime) : 1.0f / 60.0f;
        lastTime = currTime;

        ImGui::NewFrame();
        manager.DefineWindows();
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
        if (!vertexBuffer || totalVtxSize > vertexBuffer->Size()) {
            vertexBuffer = device.AllocateBuffer(totalVtxSize,
                vk::BufferUsageFlagBits::eVertexBuffer,
                VMA_MEMORY_USAGE_CPU_TO_GPU);
        }

        totalIdxSize = CeilToPowerOfTwo(totalIdxSize);
        if (!indexBuffer || totalIdxSize > indexBuffer->Size()) {
            indexBuffer = device.AllocateBuffer(totalIdxSize,
                vk::BufferUsageFlagBits::eIndexBuffer,
                VMA_MEMORY_USAGE_CPU_TO_GPU);
        }

        if (totalVtxSize == 0 || totalIdxSize == 0) return;

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
                    cmd.SetTexture(0, 0, texture);

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

    const std::string &GuiRenderer::Name() const {
        return manager.Name();
    }

} // namespace sp::vulkan
