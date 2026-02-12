/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "assets/Asset.hh"
#include "assets/AssetManager.hh"
#include "common/Defer.hh"
#include "ecs/EcsImpl.hh"
#include "graphics/GenericCompositor.hh"
#include "gui/GuiContext.hh"
#include "gui/ImGuiHelpers.hh"

#include <imgui.h>
#include <imgui_internal.h>
#include <iomanip>
#include <sstream>

namespace sp::scripts {
    using namespace ecs;

    struct SignalDisplayGui {
        std::string suffix = "mW";
        ImGuiContext *imCtx = nullptr;
        shared_ptr<ImFontAtlas> fontAtlas;

        void Init(ScriptState &state) {
            Debugf("Created signal display: %llu", state.GetInstanceId());

            imCtx = ImGui::CreateContext();
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

            uint8_t *fontData;
            int fontWidth, fontHeight;
            fontAtlas->GetTexDataAsRGBA32(&fontData, &fontWidth, &fontHeight);
        }

        void Destroy(ScriptState &state) {
            Debugf("Destroying signal display: %llu", state.GetInstanceId());
            if (imCtx) {
                ImGui::DestroyContext(imCtx);
                imCtx = nullptr;
            }
        }

        bool PreDefine(ScriptState &state, Entity ent) {
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            GuiContext::PushFont(GuiFont::Monospace, 32);
            return true;
        }

        void DefineContents(ScriptState &state, Entity ent) {
            ZoneScoped;
            auto lock = StartTransaction<ReadSignalsLock>();

            std::string text = "error";
            ImVec4 textColor(1, 0, 0, 1);
            if (ent.Exists(lock)) {
                auto maxValue = SignalRef(ent, "max_value").GetSignal(lock);
                auto value = SignalRef(ent, "value").GetSignal(lock);
                textColor.x = SignalRef(ent, "text_color_r").GetSignal(lock);
                textColor.y = SignalRef(ent, "text_color_g").GetSignal(lock);
                textColor.z = SignalRef(ent, "text_color_b").GetSignal(lock);
                std::stringstream ss;
                if (maxValue != 0.0) {
                    ss << std::fixed << std::setprecision(2) << (value / maxValue * 100.0) << "%";
                } else {
                    ss << std::fixed << std::setprecision(2) << value << suffix;
                }
                text = ss.str();
            }

            ImGui::PushStyleColor(ImGuiCol_Text, textColor);
            ImGui::PushStyleColor(ImGuiCol_Border, textColor);
            ImGui::BeginChild("signal_display", ImVec2(-FLT_MIN, -FLT_MIN), true);

            auto textSize = ImGui::CalcTextSize(text.c_str());
            ImGui::SetCursorPosX((ImGui::GetWindowSize().x - textSize.x) * 0.5f);
            ImGui::SetCursorPosY((ImGui::GetWindowSize().y - textSize.y) * 0.5f);
            ImGui::TextUnformatted(text.c_str());

            ImGui::EndChild();
            ImGui::PopStyleColor(2);
        }

        void PostDefine(ScriptState &state, Entity ent) {
            ImGui::PopFont();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor();
        }

        bool BeforeFrame(GenericCompositor &, ScriptState &state, Entity ent) {
            return true;
        }

        void RenderGui(GenericCompositor &,
            ScriptState &state,
            Entity ent,
            glm::vec2 displaySize,
            glm::vec2 scale,
            float deltaTime,
            GuiDrawData &result) {
            ZoneScoped;
            if (!imCtx) return;
            ImGui::SetCurrentContext(imCtx);
            ImGuiIO &io = ImGui::GetIO();
            io.IniFilename = nullptr;
            io.DisplaySize = ImVec2(displaySize.x, displaySize.y);
            io.DisplayFramebufferScale = ImVec2(scale.x, scale.y);
            io.DeltaTime = deltaTime;
            io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

            auto lastFonts = io.Fonts;
            io.Fonts = fontAtlas.get();
            io.Fonts->TexID = GenericCompositor::FontAtlasID;
            Defer defer([&] {
                io.Fonts = lastFonts;
            });

            ImGui::NewFrame();

            // DefineWindows
            int flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;

            if (PreDefine(state, ent)) {
                ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
                ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y));
                ImGui::Begin("signal_display", nullptr, flags);
                DefineContents(state, ent);
                ImGui::End();
                PostDefine(state, ent);
            }

            ImGui::Render();

            auto *drawData = ImGui::GetDrawData();
            drawData->ScaleClipRects(io.DisplayFramebufferScale);
            ConvertImDrawData(drawData, &result);
        }
    };
    StructMetadata MetadataSignalDisplay(typeid(SignalDisplayGui),
        sizeof(SignalDisplayGui),
        "SignalDisplayGui",
        "",
        StructField::New("suffix", &SignalDisplayGui::suffix));
    GuiScript<SignalDisplayGui> signalDisplay("signal_display", MetadataSignalDisplay);
} // namespace sp::scripts
