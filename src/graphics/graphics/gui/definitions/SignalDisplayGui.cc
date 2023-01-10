#include "SignalDisplayGui.hh"

#include "ecs/EcsImpl.hh"

#include <imgui/imgui.h>

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
            auto threshold = ecs::SignalBindings::GetSignal(lock, ent, "threshold");
            text = std::format("{:.2f}mW", value);
            if (value >= threshold) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 1, 0, 1));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0, 0, 1));
            }
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0, 0, 1));
        }
        auto textWidth = ImGui::CalcTextSize(text.c_str()).x;
        ImGui::SetCursorPosX((ImGui::GetWindowSize().x - textWidth) * 0.5f);
        ImGui::Text("%s", text.c_str());
        ImGui::PopStyleColor();
    }
} // namespace sp
