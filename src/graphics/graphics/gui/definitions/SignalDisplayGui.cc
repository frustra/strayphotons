/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "SignalDisplayGui.hh"

#include "console/CVar.hh"
#include "ecs/EcsImpl.hh"

#include <algorithm>
#include <imgui/imgui.h>
#include <iomanip>
#include <sstream>

namespace sp {
    static CVar<size_t> CVarSignalDisplayGraphHistory("g.SignalDisplayGraphHistory",
        2000,
        "The number of samples to store for graphing signals");

    SignalDisplayGui::SignalDisplayGui(const string &name, const ecs::Entity &ent)
        : GuiWindow(name), signalEntity(ent), valueHistory(CVarSignalDisplayGraphHistory.Get(), 0.0f) {}

    void SignalDisplayGui::PreDefine() {
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        PushFont(Font::Monospace, 32);
    }

    void SignalDisplayGui::PostDefine() {
        ImGui::PopFont();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
    }

    void SignalDisplayGui::DefineContents() {
        ZoneScoped;
        auto lock = ecs::StartTransaction<ecs::ReadSignalsLock>();

        auto ent = signalEntity.Get(lock);
        std::string text = "error";
        ImVec4 textColor(1, 0, 0, 1);
        double value = 0.0;
        double maxValue = 0.0;
        if (ent.Exists(lock)) {
            maxValue = ecs::SignalRef(ent, "max_value").GetSignal(lock);
            value = ecs::SignalRef(ent, "value").GetSignal(lock);
            textColor.x = ecs::SignalRef(ent, "text_color_r").GetSignal(lock);
            textColor.y = ecs::SignalRef(ent, "text_color_g").GetSignal(lock);
            textColor.z = ecs::SignalRef(ent, "text_color_b").GetSignal(lock);
            std::stringstream ss;
            if (maxValue != 0.0) {
                ss << std::fixed << std::setprecision(2) << (value / maxValue * 100.0) << "%";
            } else {
                ss << std::fixed << std::setprecision(2) << value << "mW";
            }
            text = ss.str();
        }

        ImGui::PushStyleColor(ImGuiCol_Text, textColor);
        ImGui::PushStyleColor(ImGuiCol_Border, textColor);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4());
        ImGui::BeginChild(name.c_str(), ImVec2(-FLT_MIN, -FLT_MIN), true);

        // auto textSize = ImGui::CalcTextSize(text.c_str());
        // ImGui::SetCursorPosX((ImGui::GetWindowSize().x - textSize.x) * 0.5f);
        // ImGui::SetCursorPosY((ImGui::GetWindowSize().y - textSize.y) * 0.5f);
        // ImGui::Text("%s", text.c_str());

        valueHistory.erase(valueHistory.begin());
        valueHistory.emplace_back(value);

        float minValue;
        if (maxValue != 0.0) {
            minValue = 0.0f;
        } else {
            minValue = std::min(0.0f, std::ranges::min(valueHistory));
            maxValue = std::max(1.0f, std::ranges::max(valueHistory));
        }

        ImGui::SetNextItemWidth(-1);
        auto graphName = "##graph_" + signalEntity.Name().String();
        ImGui::PlotLines(graphName.c_str(),
            valueHistory.data(),
            valueHistory.size(),
            0,
            text.c_str(),
            minValue - 0.1,
            maxValue + 0.1,
            ImVec2(0, 50));

        ImGui::EndChild();
        ImGui::PopStyleColor(3);
    }
} // namespace sp
