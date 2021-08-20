#include "GuiRenderer.hh"

#include "assets/Asset.hh"
#include "assets/AssetManager.hh"
#include "graphics/opengl/GenericShaders.hh"
#include "graphics/opengl/GlfwGraphicsContext.hh"
#include "graphics/opengl/PerfTimer.hh"
#include "graphics/opengl/ShaderManager.hh"
#include "graphics/gui/GuiManager.hh"
#include "graphics/opengl/voxel_renderer/VoxelRenderer.hh"

#include <algorithm>
#include <imgui/imgui.h>

// GLFW must be included after glew.h (Graphics.hh)
#include <GLFW/glfw3.h>

namespace sp {
    GuiRenderer::GuiRenderer(VoxelRenderer &renderer, GraphicsContext &context, GuiManager *manager)
        : parent(renderer), manager(manager) {
        manager->SetGuiContext();
        ImGuiIO &io = ImGui::GetIO();

        io.ImeWindowHandle = context.Win32WindowHandle();
        io.IniFilename = nullptr;

        std::pair<shared_ptr<const Asset>, float> fontAssets[] = {
            std::make_pair(GAssets.Load("fonts/DroidSans.ttf"), 16.0f),
            std::make_pair(GAssets.Load("fonts/3270Medium.ttf"), 32.0f),
            std::make_pair(GAssets.Load("fonts/3270Medium.ttf"), 25.0f),
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
            Assert(asset != nullptr, "Failed to load gui font");
            ImFontConfig cfg;
            cfg.FontData = (void *)asset->buffer.data();
            cfg.FontDataSize = asset->buffer.size();
            cfg.FontDataOwnedByAtlas = false;
            cfg.SizePixels = pair.second;
            cfg.GlyphRanges = &glyphRanges[0];
            memcpy(cfg.Name, asset->path.c_str(), std::min(sizeof(cfg.Name), asset->path.length()));
            io.Fonts->AddFont(&cfg);
        }

#define OFFSET(typ, field) ((size_t) & (((typ *)nullptr)->field))

        vertices.Create()
            .CreateVAO()
            .EnableAttrib(0, 2, GL_FLOAT, false, OFFSET(ImDrawVert, pos), sizeof(ImDrawVert))
            .EnableAttrib(1, 2, GL_FLOAT, false, OFFSET(ImDrawVert, uv), sizeof(ImDrawVert))
            .EnableAttrib(2, 4, GL_UNSIGNED_BYTE, true, OFFSET(ImDrawVert, col), sizeof(ImDrawVert));

        indices.Create();

#undef OFFSET

        unsigned char *fontData;
        int fontWidth, fontHeight;
        io.Fonts->GetTexDataAsRGBA32(&fontData, &fontWidth, &fontHeight);

        fontTex.Create()
            .Filter(GL_LINEAR, GL_LINEAR)
            .Size(fontWidth, fontHeight)
            .Storage(PF_RGBA8)
            .Image2D(fontData, 0, fontWidth, fontHeight);

        io.Fonts->TexID = (void *)(intptr_t)fontTex.handle;
    }

    GuiRenderer::~GuiRenderer() {}

    void GuiRenderer::Render(ecs::View view) {
        RenderPhase phase("GuiRender", parent.timer);

        manager->SetGuiContext();
        ImGuiIO &io = ImGui::GetIO();

        io.DisplaySize = ImVec2(view.extents.x, view.extents.y);

        double currTime = glfwGetTime();
        io.DeltaTime = lastTime > 0.0 ? (float)(currTime - lastTime) : 1.0f / 60.0f;
        lastTime = currTime;

        manager->BeforeFrame();
        ImGui::NewFrame();
        manager->DefineWindows();
        ImGui::Render();

        auto drawData = ImGui::GetDrawData();

        drawData->ScaleClipRects(io.DisplayFramebufferScale);

        glViewport(view.offset.x, view.offset.y, view.extents.x, view.extents.y);

        parent.shaders.Get<BasicOrthoVS>()->SetViewport(view.extents.x, view.extents.y);
        parent.ShaderControl->BindPipeline<BasicOrthoVS, BasicOrthoFS>();

        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_SCISSOR_TEST);

        vertices.BindVAO();
        indices.BindElementArray();

        for (int i = 0; i < drawData->CmdListsCount; i++) {
            const auto cmdList = drawData->CmdLists[i];
            const ImDrawIdx *offset = 0;

            vertices.SetElements(cmdList->VtxBuffer.size(), &cmdList->VtxBuffer.front(), GL_STREAM_DRAW);
            indices.SetElements(cmdList->IdxBuffer.size(), &cmdList->IdxBuffer.front(), GL_STREAM_DRAW);

            for (const auto &pcmd : cmdList->CmdBuffer) {
                if (pcmd.UserCallback) {
                    pcmd.UserCallback(cmdList, &pcmd);
                } else {
                    auto texID = (GLuint)(intptr_t)pcmd.TextureId;
                    glBindTextures(0, 1, &texID);

                    glScissor((int)pcmd.ClipRect.x,
                              (int)(view.extents.y - pcmd.ClipRect.w),
                              (int)(pcmd.ClipRect.z - pcmd.ClipRect.x),
                              (int)(pcmd.ClipRect.w - pcmd.ClipRect.y));

                    GLenum elemType = sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
                    glDrawElements(GL_TRIANGLES, pcmd.ElemCount, elemType, offset);
                }
                offset += pcmd.ElemCount;
            }
        }

        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_BLEND);
    }
} // namespace sp
