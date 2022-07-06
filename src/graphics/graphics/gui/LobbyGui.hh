#pragma once

#include "graphics/gui/GuiContext.hh"

#include <imgui/imgui.h>

namespace sp {
    class LobbyGui : public GuiWindow {
    public:
        LobbyGui(const string &name) : GuiWindow(name) {}
        virtual ~LobbyGui() {}

        void DefineContents() {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {30, 20});
            PushFont(Font::Primary, 32);
            ImGui::Button("Register");
            ImGui::PopFont();
            ImGui::PopStyleVar();
        }
    };
} // namespace sp
