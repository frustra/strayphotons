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
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        PushFont(Font::Monospace, 32);
    }

    void SignalDisplayGui::PostDefine() {
        ImGui::PopFont();
        ImGui::PopStyleColor(2);
    }

    void SignalDisplayGui::DefineContents() {
        ZoneScoped;
        auto lock = ecs::StartTransaction<ecs::ReadSignalsLock>();

        auto ent = signalEntity.Get(lock);
        std::string text = "error";
        if (ent.Exists(lock)) {
            auto value = ecs::SignalBindings::GetSignal(lock, ent, "value");
            ImVec4 textColor(0, 0, 0, 1);
            textColor.x = ecs::SignalBindings::GetSignal(lock, ent, "text_color_r");
            textColor.y = ecs::SignalBindings::GetSignal(lock, ent, "text_color_g");
            textColor.z = ecs::SignalBindings::GetSignal(lock, ent, "text_color_b");
            std::stringstream ss;
            ss << std::fixed << std::setprecision(2) << value << "mW";
            text = ss.str();
            ImGui::PushStyleColor(ImGuiCol_Text, textColor);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0, 0, 1));
        }
        auto textWidth = ImGui::CalcTextSize(text.c_str()).x;
        ImGui::SetCursorPosX((ImGui::GetWindowSize().x - textWidth) * 0.5f);
        ImGui::Text("%s", text.c_str());
        ImGui::PopStyleColor();
    }
} // namespace sp
