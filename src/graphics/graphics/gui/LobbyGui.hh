#pragma once

#include "graphics/gui/GuiManager.hh"

#include <imgui/imgui.h>

namespace sp {
    class LobbyGui : public GuiWindow {
    public:
        LobbyGui(const string &name) : GuiWindow(name) {}
        virtual ~LobbyGui() {}

        void DefineContents() {
            ImGui::Text("test test test");
        }
    };
} // namespace sp
