/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "MenuGuiManager.hh"

#include "assets/AssetManager.hh"
#include "console/CVar.hh"
#include "console/Console.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "graphics/GraphicsManager.hh"
#include "graphics/core/GraphicsContext.hh"
#include "graphics/core/Texture.hh"
#include "input/BindingNames.hh"

#include <glm/glm.hpp>
#include <imgui/imgui.h>
#include <sstream>

namespace sp {
    static CVar<bool> CVarMenuOpen("g.MenuOpen", 0, "Display pause menu");
    static CVar<bool> CVarMenuDebugCursor("g.MenuDebugCursor", false, "Force the cursor to be drawn in menus");

    MenuGuiManager::MenuGuiManager(GraphicsManager &graphics) : SystemGuiManager("menu"), graphics(graphics) {
        {
            auto lock = ecs::StartTransaction<ecs::Read<ecs::Name>, ecs::Write<ecs::EventInput, ecs::FocusLock>>();

            auto gui = guiEntity.Get(lock);
            Assertf(gui.Has<ecs::EventInput>(lock),
                "Expected menu gui to start with an EventInput: %s",
                guiEntity.Name().String());

            auto &eventInput = gui.Get<ecs::EventInput>(lock);
            eventInput.Register(lock, events, INPUT_EVENT_MENU_OPEN);
            eventInput.Register(lock, events, INPUT_EVENT_MENU_BACK);
        }
    }

    void MenuGuiManager::BeforeFrame() {
        SystemGuiManager::BeforeFrame();

        ImGui::StyleColorsClassic();

        ImGuiIO &io = ImGui::GetIO();

        bool focusChanged = false;
        {
            auto lock = ecs::StartTransaction<ecs::ReadSignalsLock, ecs::Read<ecs::EventInput>>();

            ecs::Event event;
            while (ecs::EventInput::Poll(lock, events, event)) {
                if (event.name == INPUT_EVENT_MENU_OPEN) {
                    selectedScreen = MenuScreen::Main;
                    CVarMenuOpen.Set(true);
                } else if (event.name == INPUT_EVENT_MENU_BACK) {
                    if (selectedScreen == MenuScreen::Main) {
                        CVarMenuOpen.Set(false);
                    } else {
                        selectedScreen = MenuScreen::Main;
                    }
                }
            }

            auto &focusLock = lock.Get<ecs::FocusLock>();
            focusChanged = MenuOpen() != focusLock.HasFocus(ecs::FocusLayer::Menu);
        }

        io.MouseDrawCursor = CVarMenuDebugCursor.Get();

        if (focusChanged) {
            auto lock = ecs::StartTransaction<ecs::Write<ecs::FocusLock>>();
            auto &focusLock = lock.Get<ecs::FocusLock>();
            if (MenuOpen()) {
                focusLock.AcquireFocus(ecs::FocusLayer::Menu);
            } else {
                focusLock.ReleaseFocus(ecs::FocusLayer::Menu);
            }
        }
    }

    static bool StringVectorGetter(void *data, int idx, const char **out_text) {
        auto vec = (vector<string> *)data;
        if (out_text) {
            *out_text = vec->at(idx).c_str();
        }
        return true;
    }

    static bool IsAspect(glm::ivec2 size, int w, int h) {
        return ((size.x * h) / w) == size.y;
    }

    static vector<string> MakeResolutionLabels(const vector<glm::ivec2> &modes) {
        vector<string> labels;
        for (size_t i = 0; i < modes.size(); i++) {
            auto m = modes[i];
            std::stringstream str;
            str << m.x << "x" << m.y;

            if (IsAspect(m, 16, 9)) str << " (16:9)";
            if (IsAspect(m, 16, 10)) str << " (16:10)";
            if (IsAspect(m, 4, 3)) str << " (4:3)";

            labels.push_back(str.str());
        }
        return labels;
    }

    void MenuGuiManager::DefineWindows() {
        ZoneScoped;
        SetGuiContext();
        ImGuiIO &io = ImGui::GetIO();

        // ImGui::ShowTestWindow();

        ImVec4 empty(0.0, 0.0, 0.0, 0.0);
        ImVec4 black(0.0, 0.0, 0.0, 1.0);
        ImVec4 white(1.0, 1.0, 1.0, 1.0);
        ImVec4 green(0.05, 1.0, 0.3, 1.0);

        ImGui::PushStyleColor(ImGuiCol_Button, empty);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, green);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, green);
        ImGui::PushStyleColor(ImGuiCol_Text, white);
        ImGui::PushStyleColor(ImGuiCol_TextButtonHover, black);
        ImGui::PushStyleColor(ImGuiCol_TextButtonActive, black);
        ImGui::PushStyleColor(ImGuiCol_CheckMark, green);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, black);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0);
        PushFont(Font::Monospace, 25);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoCollapse |
                                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar |
                                 ImGuiWindowFlags_AlwaysAutoResize;

        if (!logoTex) logoTex = graphics.context->LoadTexture(Assets()->LoadImage("logos/sp-menu.png")->Get());
        ImVec2 logoSize(logoTex->GetWidth() * 0.5f, logoTex->GetHeight() * 0.5f);

        if (selectedScreen == MenuScreen::Main) {
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                ImGuiCond_Always,
                ImVec2(0.5f, 0.5f));
            ImGui::Begin("MenuMain", nullptr, flags);

            ImGui::Image((void *)logoTex->GetHandle(), logoSize);

            if (ImGui::Button("Resume")) {
                CVarMenuOpen.Set(false);
            }

            if (ImGui::Button("Scene Select")) {
                selectedScreen = MenuScreen::SceneSelect;
            }

            if (ImGui::Button("Options")) {
                selectedScreen = MenuScreen::Options;
            }

            if (ImGui::Button("Quit")) {
                GetConsoleManager()->QueueParseAndExecute("exit");
            }

            ImGui::End();
        } else if (selectedScreen == MenuScreen::SceneSelect) {
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                ImGuiCond_Always,
                ImVec2(0.5f, 0.5f));
            ImGui::Begin("MenuSceneSelect", nullptr, flags);

            ImGui::Image((void *)logoTex->GetHandle(), logoSize);

            ImGui::Text("Scene Select");
            ImGui::Text(" ");

            PushFont(Font::Monospace, 32);

#define LEVEL_BUTTON(name, file)                                      \
    if (ImGui::Button(name)) {                                        \
        CVarMenuOpen.Set(false);                                      \
        selectedScreen = MenuScreen::Main;                            \
        GetConsoleManager()->QueueParseAndExecute("loadscene " file); \
    }

            LEVEL_BUTTON("01 - Outside", "01-outside")
            LEVEL_BUTTON("02 - Mirrors", "02-mirrors")
            LEVEL_BUTTON("03 - Dark", "03-dark")
            LEVEL_BUTTON("04 - Symmetry", "04-symmetry")
            LEVEL_BUTTON("Sponza", "sponza")
            LEVEL_BUTTON("Station Center", "station-center")
            LEVEL_BUTTON("Cornell Box", "cornell-box-1")
            LEVEL_BUTTON("Cornell Box Mirror", "cornell-box-3")
            LEVEL_BUTTON("Test 1", "test1")

#undef LEVEL_BUTTON

            ImGui::PopFont();
            ImGui::Text(" ");

            if (ImGui::Button("Back")) {
                selectedScreen = MenuScreen::Main;
            }

            ImGui::End();
        } else if (selectedScreen == MenuScreen::Options) {
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                ImGuiCond_Always,
                ImVec2(0.5f, 0.5f));
            ImGui::Begin("MenuOptions", nullptr, flags);

            ImGui::Image((void *)logoTex->GetHandle(), logoSize);

            ImGui::Text("Options");
            ImGui::Text(" ");
            ImGui::Columns(2, "optcols", false);

            PushFont(Font::Monospace, 32);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 15));

            ImGui::Text("Resolution");
            ImGui::Text("Full Screen");
            ImGui::Text("Mirror VR View");

            ImGui::PopStyleVar();
            ImGui::NextColumn();
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 10));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0);

            {
                auto modes = graphics.context->MonitorModes();
                auto size = CVarWindowSize.Get();
                // If the mode isn't in the list, refresh it, and then add the current resolution to the bottom if
                // not found.
                int resIndex = std::find(modes.begin(), modes.end(), size) - modes.begin();
                if (resIndex < 0 || resIndex >= (int)modes.size()) {
                    resIndex = modes.size();
                    modes.push_back(size);
                }

                vector<string> resLabels = MakeResolutionLabels(modes);

                ImGui::PushItemWidth(300.0f);
                if (ImGui::Combo("##respicker", &resIndex, StringVectorGetter, &resLabels, modes.size())) {
                    CVarWindowSize.Set(modes[resIndex]);
                }
                ImGui::PopItemWidth();

                bool fullscreen = CVarWindowFullscreen.Get();
                if (ImGui::Checkbox("##fullscreencheck", &fullscreen)) {
                    CVarWindowFullscreen.Set(fullscreen);
                }

                auto &mirrorXRCVar = GetConsoleManager()->GetCVar<bool>("r.mirrorxr");
                bool mirrorXR = mirrorXRCVar.Get();
                if (ImGui::Checkbox("##mirrorxrcheck", &mirrorXR)) {
                    mirrorXRCVar.Set(mirrorXR);
                }
            }

            ImGui::PopStyleVar(2);
            ImGui::PopFont();
            ImGui::Columns(1);
            ImGui::Text(" ");

            if (ImGui::Button("Done")) {
                selectedScreen = MenuScreen::Main;
            }

            ImGui::End();
        }

        ImGui::PopFont();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(8);
    }

    bool MenuGuiManager::MenuOpen() const {
        return CVarMenuOpen.Get();
    }
} // namespace sp
