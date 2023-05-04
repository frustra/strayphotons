#include "SignalDisplayGui.hh"

#include "ecs/EcsImpl.hh"

#include <imgui/imgui.h>
#include <iomanip>
#include <sstream>

namespace sp {
    SignalDisplayGui::SignalDisplayGui(const string &name, const ecs::Entity &ent)
        : GuiWindow(name), signalEntity(ent) {}

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

        static const ecs::StringHandle maxValueHandle = ecs::GetStringHandler().Get("max_value");
        static const ecs::StringHandle valueHandle = ecs::GetStringHandler().Get("value");
        static const ecs::StringHandle textColorRHandle = ecs::GetStringHandler().Get("text_color_r");
        static const ecs::StringHandle textColorGHandle = ecs::GetStringHandler().Get("text_color_g");
        static const ecs::StringHandle textColorBHandle = ecs::GetStringHandler().Get("text_color_b");

        auto ent = signalEntity.Get(lock);
        std::string text = "error";
        ImVec4 textColor(1, 0, 0, 1);
        if (ent.Exists(lock)) {
            auto maxValue = ecs::SignalBindings::GetSignal(lock, ent, maxValueHandle);
            auto value = ecs::SignalBindings::GetSignal(lock, ent, valueHandle);
            textColor.x = ecs::SignalBindings::GetSignal(lock, ent, textColorRHandle);
            textColor.y = ecs::SignalBindings::GetSignal(lock, ent, textColorGHandle);
            textColor.z = ecs::SignalBindings::GetSignal(lock, ent, textColorBHandle);
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
        ImGui::BeginChild(name.c_str(), ImVec2(-FLT_MIN, -FLT_MIN), true);

        auto textSize = ImGui::CalcTextSize(text.c_str());
        ImGui::SetCursorPosX((ImGui::GetWindowSize().x - textSize.x) * 0.5f);
        ImGui::SetCursorPosY((ImGui::GetWindowSize().y - textSize.y) * 0.5f);
        ImGui::Text("%s", text.c_str());

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
    }
} // namespace sp
