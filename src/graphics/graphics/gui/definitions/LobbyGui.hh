/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "graphics/gui/GuiContext.hh"

#include <imgui/imgui.h>

namespace sp {
    class LobbyGui final : public GuiRenderable {
    public:
        LobbyGui(const string &name) : GuiRenderable(name, GuiLayoutAnchor::Fullscreen) {}
        virtual ~LobbyGui() {}

        enum class State { Initial, Page1 };

        State state = State::Initial;

        void DefineContents() {
            ZoneScoped;
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {30, 20});
            PushFont(GuiFont::Primary, 32);
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
