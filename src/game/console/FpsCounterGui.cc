/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "FpsCounterGui.hh"

#include "common/RegisteredThread.hh"
#include "ecs/EcsImpl.hh"

#include <imgui/imgui.h>
#include <iomanip>
#include <sstream>

namespace sp {
    static CVar<bool> CVarShowFPS("r.ShowFPS", false, "Show the frame rate of each system on screen");
    static CVar<int> CVarShowFPSCorner("r.ShowFPSCorner",
        1,
        "Specify which corner to draw show the FPS counter in (number from 0 to 3)");

    // Why is this window still resizable??
    FpsCounterGui::FpsCounterGui()
        : ecs::GuiDefinition("fps_counter",
              ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                  ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoTitleBar |
                  ImGuiWindowFlags_NoBringToFrontOnFocus) {}

    bool FpsCounterGui::BeforeFrame(ecs::Entity ent) {
        return CVarShowFPS.Get();
    }

    void FpsCounterGui::PreDefine(ecs::Entity ent) {
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.5f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);

        ImGuiViewport *imguiViewport = ImGui::GetMainViewport();
        Assertf(imguiViewport, "ImGui::GetMainViewport() returned null");
        ImVec2 viewportPos = imguiViewport->WorkPos;
        ImVec2 viewportSize = imguiViewport->WorkSize;
        switch (CVarShowFPSCorner.Get()) {
        case 1:
            viewportPos.x += viewportSize.x;
            ImGui::SetNextWindowPos(viewportPos, 0, ImVec2(1, 0));
            break;
        case 2:
            viewportPos.y += viewportSize.y;
            ImGui::SetNextWindowPos(viewportPos, 0, ImVec2(0, 1));
            break;
        case 3:
            viewportPos.x += viewportSize.x;
            viewportPos.y += viewportSize.y;
            ImGui::SetNextWindowPos(viewportPos, 0, ImVec2(1, 1));
            break;
        case 0:
        default:
            ImGui::SetNextWindowPos(viewportPos);
            break;
        }
        ImGui::SetNextWindowSizeConstraints(ImVec2(-1.0f, -1.0f), ImVec2(viewportSize.x, viewportSize.y));
    }

    void FpsCounterGui::PostDefine(ecs::Entity ent) {
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }

    void FpsCounterGui::DefineContents(ecs::Entity ent) {
        ZoneScoped;
        ImGui::Text("Render FPS: %u", GetMeasuredFps("RenderThread"));
        ImGui::Text("Logic FPS: %u", GetMeasuredFps("GameLogic"));
        ImGui::Text("Physics FPS: %u", GetMeasuredFps("PhysX"));
    }
} // namespace sp
