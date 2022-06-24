#pragma once

#include "graphics/gui/GuiManager.hh"

#include <imgui/imgui.h>

namespace sp {
    class InspectorGui : public GuiWindow {
    public:
        InspectorGui(const string &name) : GuiWindow(name) {}
        virtual ~InspectorGui() {}

        void DefineContents() {
            ImGui::Text("test test test");
        }
    };
} // namespace sp
