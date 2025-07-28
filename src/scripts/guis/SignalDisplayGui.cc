/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "ecs/EcsImpl.hh"
#include "graphics/gui/GuiContext.hh"

#include <imgui/imgui.h>
#include <iomanip>
#include <sstream>

namespace sp::scripts {
    using namespace ecs;

    struct SignalDisplayGui {
        std::string suffix = "mW";

        void Init(ScriptState &state) {
            // Logf("Created signal display: %llu", state.GetInstanceId());
        }

        void Init(ScriptState &state, GuiRenderable &gui) {
            // Logf("Created signal display gui: %llu, %s %s", state.GetInstanceId(), gui.name, gui.anchor);
        }

        void Destroy(ScriptState &state) {
            // Logf("Destroying signal display: %llu", state.GetInstanceId());
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
            ImGui::Text("%s", text.c_str());

            ImGui::EndChild();
            ImGui::PopStyleColor(2);
        }

        void PostDefine(ScriptState &state, Entity ent) {
            ImGui::PopFont();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor();
        }
    };
    StructMetadata MetadataSignalDisplay(typeid(SignalDisplayGui),
        sizeof(SignalDisplayGui),
        "SignalDisplayGui",
        "",
        StructField::New("suffix", &SignalDisplayGui::suffix));
    GuiScript<SignalDisplayGui> signalDisplay("signal_display", MetadataSignalDisplay);
} // namespace sp::scripts
