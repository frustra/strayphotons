/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "SignalDisplayGui.hh"
#include "ecs/EcsImpl.hh"
#include "gui/GuiContext.hh"

#include <imgui/imgui.h>
#include <iomanip>
#include <sstream>

namespace sp {
    SignalDisplayGui::SignalDisplayGui(const string &name) : ecs::GuiDefinition(name) {}

    void SignalDisplayGui::PreDefine(ecs::Entity ent) {
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        GuiContext::PushFont(GuiFont::Monospace, 32);
    }

    void SignalDisplayGui::PostDefine(ecs::Entity ent) {
        ImGui::PopFont();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
    }

    void SignalDisplayGui::DefineContents(ecs::Entity ent) {
        ZoneScoped;
        auto lock = ecs::StartTransaction<ecs::ReadSignalsLock>();

        std::string text = "error";
        ImVec4 textColor(1, 0, 0, 1);
        if (ent.Exists(lock)) {
            auto maxValue = ecs::SignalRef(ent, "max_value").GetSignal(lock);
            auto value = ecs::SignalRef(ent, "value").GetSignal(lock);
            textColor.x = ecs::SignalRef(ent, "text_color_r").GetSignal(lock);
            textColor.y = ecs::SignalRef(ent, "text_color_g").GetSignal(lock);
            textColor.z = ecs::SignalRef(ent, "text_color_b").GetSignal(lock);
            std::stringstream ss;
            if (maxValue != 0.0) {
                ss << std::fixed << std::setprecision(2) << (value / maxValue * 100.0) << "%";
            } else {
                ss << std::fixed << std::setprecision(2) << value << " mph";
            }
            text = ss.str();
        }

        ImGui::PushStyleColor(ImGuiCol_Text, textColor);
        ImGui::PushStyleColor(ImGuiCol_Border, textColor);
        ImGui::BeginChild(name.c_str(), ImVec2(-FLT_MIN, -FLT_MIN), true);

        auto textSize = ImGui::CalcTextSize(text.c_str());
        ImGui::SetCursorPosX((ImGui::GetWindowSize().x - textSize.x) * 0.5f);
        ImGui::SetCursorPosY((ImGui::GetWindowSize().y - textSize.y) * 0.5f);
        ImGui::TextUnformatted(text.data(), text.data() + text.size());

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
    }
} // namespace sp
