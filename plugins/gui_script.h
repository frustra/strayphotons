/*
 * Stray Photons - Copyright (C) 2026 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <common/Logging.hh>
#include <cstdint>
#include <fstream>
#include <gui/ImGuiHelpers.hh>
#include <imgui.h>
#include <imgui_internal.h>
#include <implot.h>

namespace sp {
    enum class GuiFont {
        Primary,
        Accent,
        Monospace,
    };

    struct GuiFontDef {
        GuiFont type;
        const char *name;
        float size;
    };
} // namespace sp

static const ImWchar glyphRanges[] = {
    0x0020,
    0x00FF, // Basic Latin + Latin Supplement
    0x2100,
    0x214F, // Letterlike Symbols
    0,
};

static const std::array fontList = {
    sp::GuiFontDef{sp::GuiFont::Primary, "DroidSans-Regular.ttf", 16.0f},
    sp::GuiFontDef{sp::GuiFont::Primary, "DroidSans-Regular.ttf", 32.0f},
    sp::GuiFontDef{sp::GuiFont::Monospace, "3270SemiCondensed-Regular.ttf", 25.0f},
    sp::GuiFontDef{sp::GuiFont::Monospace, "3270SemiCondensed-Regular.ttf", 32.0f},
};

inline void PushFont(sp::GuiFont fontType, float fontSize) {
    auto &io = ImGui::GetIO();
    // Assert(io.Fonts->Fonts.size() == fontList.size() + 1, "unexpected font list size");

    for (size_t i = 0; i < fontList.size(); i++) {
        auto &f = fontList[i];
        if (f.type == fontType && f.size == fontSize) {
            ImGui::PushFont(io.Fonts->Fonts[i + 1]);
            return;
        }
    }

    // Abortf("missing font type %d with size %f", (int)fontType, fontSize);
}

template<typename GuiDef>
struct ScriptGuiContext {
    GuiDef gui;
    ImGuiContext *imCtx = nullptr;
    ImPlotContext *imPlot = nullptr;
    shared_ptr<ImFontAtlas> fontAtlas;
    ImFontAtlas *originalAtlas = nullptr;

    ScriptGuiContext() : gui() {}

    static void DefaultInit(void *context) {
        ScriptGuiContext<GuiDef> *ctx = static_cast<ScriptGuiContext<GuiDef> *>(context);
        new (ctx) ScriptGuiContext<GuiDef>();
    }

    static void Init(void *context, sp_script_state_t *state) {
        ScriptGuiContext<GuiDef> *ctx = static_cast<ScriptGuiContext<GuiDef> *>(context);
        size_t eventIndex = sp_event_name_vector_get_size(&state->definition.events);
        sp_event_name_vector_resize(&state->definition.events, eventIndex + 1);
        event_name_t *events = sp_event_name_vector_get_data(&state->definition.events);
        std::strncpy(events[eventIndex], "/gui/imgui_input", sizeof(events[eventIndex]) - 1);

        ctx->imCtx = ImGui::CreateContext();
        ctx->imPlot = ImPlot::CreateContext();
        ctx->fontAtlas = make_shared<ImFontAtlas>();
        ctx->fontAtlas->AddFontDefault(nullptr);

        for (auto &def : fontList) {
            // auto asset = Assets().Load("fonts/"s + def.name)->Get();
            // Assertf(asset, "Failed to load gui font %s", def.name);
            auto filename = "../assets/fonts/"s + def.name;
            std::ifstream stream(filename.c_str(), std::ios::in | std::ios::binary);

            if (stream) {
                stream.seekg(0, std::ios::end);
                auto size = stream.tellg();
                stream.seekg(0, std::ios::beg);

                std::vector<uint8_t> buffer(size);
                stream.read((char *)buffer.data(), size);
                Assertf(stream.good(), "Failed to read whole asset file: %s", filename);
                stream.close();

                ImFontConfig cfg = {};
                cfg.FontData = (void *)buffer.data();
                cfg.FontDataSize = buffer.size();
                cfg.FontDataOwnedByAtlas = false;
                cfg.SizePixels = def.size;
                cfg.GlyphRanges = &glyphRanges[0];
                strncpy(cfg.Name, filename.c_str(), std::min(sizeof(cfg.Name) - 1, filename.length()));
                ctx->fontAtlas->AddFont(&cfg);
            }
        }

        uint8_t *fontData;
        int fontWidth, fontHeight;
        ctx->fontAtlas->GetTexDataAsRGBA32(&fontData, &fontWidth, &fontHeight);

        ImGuiIO &io = ImGui::GetIO(ctx->imCtx);
        ctx->originalAtlas = io.Fonts;
        io.Fonts = ctx->fontAtlas.get();
        io.Fonts->TexID = (uint64_t)std::numeric_limits<uint32_t>::max() + 1; // FontAtlasID
        io.IniFilename = nullptr;
        io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    }

    static void Destroy(void *context, sp_script_state_t *state) {
        ScriptGuiContext<GuiDef> *ctx = static_cast<ScriptGuiContext<GuiDef> *>(context);
        ImGui::GetIO(ctx->imCtx).Fonts = ctx->originalAtlas;
        ImPlot::DestroyContext(ctx->imPlot);
        ImGui::DestroyContext(ctx->imCtx);
        ctx->imCtx = nullptr;
    }

    static bool BeforeFrameStatic(void *context,
        sp_compositor_ctx_t *compositor,
        sp_script_state_t *state,
        tecs_entity_t ent) {
        ScriptGuiContext<GuiDef> *ctx = static_cast<ScriptGuiContext<GuiDef> *>(context);
        if (!ctx->imCtx) return false;

        return ctx->gui.BeforeFrame(state, ent);
    }

    static void RenderGui(void *context,
        sp_compositor_ctx_t *compositor,
        sp_script_state_t *state,
        tecs_entity_t ent,
        vec2_t displaySize,
        vec2_t scale,
        float deltaTime,
        sp_gui_draw_data_t *result) {
        ScriptGuiContext<GuiDef> *ctx = static_cast<ScriptGuiContext<GuiDef> *>(context);
        if (!ctx->imCtx) return;
        ImGui::SetCurrentContext(ctx->imCtx);
        ImGuiIO &io = ImGui::GetIO();
        io.DisplaySize = ImVec2(displaySize.v[0], displaySize.v[1]);
        io.DisplayFramebufferScale = ImVec2(scale.v[0], scale.v[1]);
        io.DeltaTime = deltaTime;

        ImGui::NewFrame();

        // DefineWindows
        int flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;

        ctx->gui.PreDefine(state, ent);
        // static bool open = true;
        // ImPlot::ShowDemoWindow(&open);
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y));
        ImGui::Begin("signal_display", nullptr, flags);
        ctx->gui.DefineContents(compositor, state, ent);
        ImGui::End();
        ctx->gui.PostDefine(state, ent);

        ImGui::Render();

        auto *drawData = ImGui::GetDrawData();
        drawData->ScaleClipRects(io.DisplayFramebufferScale);
        sp::ConvertImDrawData(drawData, result);
    }
};
