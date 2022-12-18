#pragma once

#include "graphics/gui/GuiContext.hh"

#include <imgui/imgui.h>

namespace sp {
    class LobbyGui : public GuiWindow {
    public:
        LobbyGui(const string &name) : GuiWindow(name) {}
        virtual ~LobbyGui() {}

        enum class State { Initial, Page1 };

        State state = State::Initial;

        void DefineContents() {
            ZoneScoped;
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {30, 20});
            PushFont(Font::Primary, 32);
            if (state == State::Initial) {
                if (ImGui::Button("Check In")) state = State::Page1;
            } else if (state == State::Page1) {
                ImGui::Text("Unimplemented");
            }
            ImGui::PopFont();
            ImGui::PopStyleVar();
        }
    };
} // namespace sp
